#include "audio.h"

#include "audio_handoff.h"
#include "dsp.h"

#include <gba_base.h>
#include <gba_dma.h>
#include <gba_interrupt.h>
#include <gba_sound.h>
#include <gba_timers.h>
#include <string.h>

enum {
    TIMER0_RELOAD_16384_HZ = 0xfc00,
    TIMER1_RELOAD_AUDIO_BLOCK = 0xfe00,
    TIMER1_RELOAD_FIRST_HANDOFF = 0xfe10
};

_Static_assert((int)AUDIO_FIFO_READ_AHEAD_SAMPLES
               == (int)AUDIO_HANDOFF_DMA_SAMPLES,
               "FIFO guard and handoff request sizes must match");

#define FIFO_DMA_FLAGS \
    (DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | DMA_SPECIAL | DMA_ENABLE | 4u)

volatile u32 audio_underruns;
volatile u32 audio_blocks_rendered;
volatile u32 worst_mix_cycles;

static s8 left_buffers[2][AUDIO_BLOCK_SAMPLES + AUDIO_FIFO_GUARD_SAMPLES]
    IWRAM_DATA ALIGN(4);
static s8 right_buffers[2][AUDIO_BLOCK_SAMPLES + AUDIO_FIFO_GUARD_SAMPLES]
    IWRAM_DATA ALIGN(4);
static DspState dsp_state IWRAM_DATA ALIGN(4);
static int16_t fast_reverb_delay[DSP_FDN_LINES][DSP_FDN_MAX_DELAY]
    IWRAM_DATA ALIGN(4);
static DspReverbMemory reverb_memory EWRAM_DATA ALIGN(4);
static volatile u8 active_buffer;
static volatile u8 buffer_ready[2];
static volatile u8 first_handoff;

#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
static int diagnostic_sample;
static int diagnostic_step;
#endif

static void render_audio_block(unsigned int index)
{
#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
    int frame;
    for (frame = 0; frame < AUDIO_BLOCK_SAMPLES; ++frame) {
        left_buffers[index][frame] = (s8)diagnostic_sample;
        right_buffers[index][frame] = (s8)-diagnostic_sample;
        diagnostic_sample += diagnostic_step;
        if (diagnostic_sample >= 96) {
            diagnostic_sample = 96;
            diagnostic_step = -1;
        } else if (diagnostic_sample <= -96) {
            diagnostic_sample = -96;
            diagnostic_step = 1;
        }
    }
#else
    dsp_render(&dsp_state, (int8_t *)left_buffers[index],
               (int8_t *)right_buffers[index], AUDIO_BLOCK_SAMPLES);
#endif
}

/*
 * Direct Sound asks DMA for another four words while 16 samples are still in
 * its FIFO.  Put the start of the following block after the current block so
 * that this read-ahead remains continuous across the software buffer swap.
 */
static void copy_fifo_guard(unsigned int current, unsigned int next)
{
    u32 *left_guard = (u32 *)&left_buffers[current][AUDIO_BLOCK_SAMPLES];
    u32 *right_guard = (u32 *)&right_buffers[current][AUDIO_BLOCK_SAMPLES];
    const u32 *left_next = (const u32 *)&left_buffers[next][0];
    const u32 *right_next = (const u32 *)&right_buffers[next][0];
    unsigned int word;

    for (word = 0; word < AUDIO_FIFO_GUARD_SAMPLES / 4; ++word) {
        left_guard[word] = left_next[word];
        right_guard[word] = right_next[word];
    }
}

static void start_fifo_dma(unsigned int index, unsigned int offset)
{
    REG_DMA1SAD = (u32)&left_buffers[index][offset];
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA1CNT = FIFO_DMA_FLAGS;

    REG_DMA2SAD = (u32)&right_buffers[index][offset];
    REG_DMA2DAD = (u32)&REG_FIFO_B;
    REG_DMA2CNT = FIFO_DMA_FLAGS;
}

static void audio_block_irq(void) IWRAM_CODE;
static void audio_block_irq(void)
{
    unsigned int current = active_buffer;
    unsigned int next = current ^ 1u;
    unsigned int selected;
    unsigned int timer_reload = first_handoff
        ? TIMER1_RELOAD_FIRST_HANDOFF : TIMER1_RELOAD_AUDIO_BLOCK;
    unsigned int elapsed_samples = (u16)(REG_TM1CNT_L - timer_reload);
    unsigned int queued_samples
        = audio_handoff_resume_offset(elapsed_samples);

    if (queued_samples > AUDIO_BLOCK_SAMPLES)
        queued_samples = AUDIO_BLOCK_SAMPLES;

    /*
     * The first handoff is deliberately 16 samples early.  Rephase Timer 1
     * here so every later handoff remains one full block apart while the FIFO
     * still contains the guard copied from the following buffer.
     */
    if (first_handoff) {
        REG_TM1CNT_H = 0;
        REG_TM1CNT_L = TIMER1_RELOAD_AUDIO_BLOCK;
        REG_TM1CNT_H = TIMER_COUNT | TIMER_IRQ | TIMER_START;
        first_handoff = 0;
    }

    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;

    if (buffer_ready[next]) {
        selected = next;
        active_buffer = (u8)next;
        buffer_ready[current] = 0;
    } else {
        selected = current;
        ++audio_underruns;
    }

    /*
     * This IRQ is scheduled 16 samples before the block edge.  At the exact
     * IRQ tick the FIFO still owns the final source block; it does not request
     * the first 16 mirrored samples until the following audio tick.  Round
     * only actual IRQ lateness up to complete DMA requests, so an on-time
     * handoff resumes the next buffer at sample zero rather than dropping its
     * first 16 samples.
     */
    start_fifo_dma(selected, queued_samples);
    ++audio_blocks_rendered;
}

void audio_init(const int8_t *sample, u32 length,
                const ParameterState *parameters, u32 random_seed,
                int startup_center, int startup_grains)
{
    int line;
    audio_underruns = 0;
    audio_blocks_rendered = 0;
    worst_mix_cycles = 0;
    dsp_init(&dsp_state, &reverb_memory);
    memset(fast_reverb_delay, 0, sizeof(fast_reverb_delay));
    for (line = 0; line < DSP_FDN_LINES; ++line)
        dsp_set_reverb_line(&dsp_state, line, fast_reverb_delay[line]);
    dsp_seed_random(&dsp_state, random_seed);
    dsp_set_parameters(&dsp_state, parameters);
    dsp_set_sample(&dsp_state, sample, length);
    if (startup_center >= 0 && startup_grains > 0)
        dsp_trigger_burst_count(
            &dsp_state, startup_center, startup_grains);

#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
    diagnostic_sample = -96;
    diagnostic_step = 1;
#endif
    render_audio_block(0);
    render_audio_block(1);
    copy_fifo_guard(0, 1);
    active_buffer = 0;
    buffer_ready[0] = 1;
    buffer_ready[1] = 1;
    first_handoff = 1;

    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;

    REG_SOUNDCNT_X = 0x0080;
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = SNDA_VOL_100 | SNDB_VOL_100 |
                     SNDA_L_ENABLE | SNDB_R_ENABLE |
                     SNDA_RESET_FIFO | SNDB_RESET_FIFO;

    irqSet(IRQ_TIMER1, audio_block_irq);
    irqEnable(IRQ_TIMER1);

    REG_TM0CNT_L = TIMER0_RELOAD_16384_HZ;
    REG_TM1CNT_L = TIMER1_RELOAD_FIRST_HANDOFF;

    start_fifo_dma(0, 0);

    REG_TM1CNT_H = TIMER_COUNT | TIMER_IRQ | TIMER_START;
    REG_TM0CNT_H = TIMER_START;
}

void audio_service(void)
{
    unsigned int target = active_buffer ^ 1u;
    u16 old_ime;
    u32 elapsed;

    if (buffer_ready[target])
        return;

    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TIMER_COUNT | TIMER_START;
    REG_TM2CNT_H = TIMER_START;
    render_audio_block(target);
    elapsed = ((u32)REG_TM3CNT_L << 16) | REG_TM2CNT_L;
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;

    if ((u32)elapsed > worst_mix_cycles)
        worst_mix_cycles = elapsed;

    /* Publish the completed block only after the active guard is coherent. */
    old_ime = REG_IME;
    REG_IME = 0;
    if (target == (active_buffer ^ 1u) && !buffer_ready[target]) {
        copy_fifo_guard(active_buffer, target);
        buffer_ready[target] = 1;
    }
    REG_IME = old_ime;
}

void audio_set_parameters(const ParameterState *parameters)
{
    dsp_set_parameters(&dsp_state, parameters);
}

void audio_set_sample(const int8_t *sample, u32 length)
{
    dsp_set_sample(&dsp_state, sample, length);
}

void audio_trigger_burst(int center_x)
{
    dsp_trigger_burst(&dsp_state, center_x);
}

void audio_set_gate(int active, int center_x)
{
    dsp_set_gate(&dsp_state, active, center_x);
}

int audio_pop_marker(u8 *x)
{
    return dsp_pop_marker(&dsp_state, x);
}

u32 audio_grains_started(void)
{
    return dsp_state.grains_started;
}
