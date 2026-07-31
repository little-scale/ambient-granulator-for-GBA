#include "audio.h"
#include "gfx.h"
#include "startup.h"
#include "ui.h"

#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_timers.h>
#include <gba_video.h>

static volatile u32 video_frames;
#ifndef AMBIENT_FIFO_CONTINUITY_PROFILE
static UiState ui_state EWRAM_DATA;
#endif

static void vblank_irq(void)
{
    ++video_frames;
}

static void commit_frame(void)
{
    int segment;
    for (segment = 0; segment < 4; ++segment) {
        gfx_commit_segment(segment);
        audio_service();
    }
}

#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
static void run_fifo_continuity_profile(void)
{
    u32 displayed_frame = 0;
    ParameterState parameters;

    irqInit();
    irqSet(IRQ_VBLANK, vblank_irq);
    irqEnable(IRQ_VBLANK);

    gfx_init();
    parameters_reset(&parameters);
    audio_init(0, 0, &parameters, 0, -1, 0);
    gfx_clear(COLOR_BLACK);
    commit_frame();

    for (;;) {
        audio_service();
        if (video_frames != displayed_frame) {
            displayed_frame = video_frames;
            gfx_present();
            audio_service();
            commit_frame();
        }
    }
}
#endif

int main(void)
{
#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
    run_fifo_continuity_profile();
    return 0;
#else
    u32 displayed_frame = 0;
    int frame_pending = 1;
    int bank_valid;
    u32 startup_entropy;
    u32 startup_seed;

    REG_TM2CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM2CNT_H = TIMER_START;
    irqInit();
    irqSet(IRQ_VBLANK, vblank_irq);
    irqEnable(IRQ_VBLANK);

    gfx_init();
    setRepeat(18, 4);
    startup_entropy = (u32)REG_TM2CNT_L
        ^ ((u32)REG_VCOUNT << 16)
        ^ ((u32)REG_KEYINPUT << 1);
    REG_TM2CNT_H = 0;
    startup_seed = startup_next_seed(startup_entropy);
    bank_valid = ui_init(&ui_state, startup_seed);
    audio_init(bank_valid ? ui_state.sample.pcm : 0,
               bank_valid ? ui_state.sample.length : 0,
               &ui_state.parameters, startup_seed,
               bank_valid ? ui_state.position : -1,
               bank_valid ? STARTUP_GRAIN_COUNT : 0);
    ui_render(&ui_state);
    ui_state.dirty = 0;
    commit_frame();

    for (;;) {
        audio_service();

        if (video_frames != displayed_frame) {
            displayed_frame = video_frames;
            if (frame_pending) {
                gfx_present();
                frame_pending = 0;
            }
            audio_service();
            scanKeys();
            ui_handle_input(&ui_state, keysHeld(), keysDown(), keysUp(),
                            keysDownRepeat());
            ui_tick(&ui_state);
            if (ui_state.dirty) {
                audio_service();
                ui_render(&ui_state);
                audio_service();
                commit_frame();
                ui_state.dirty = 0;
                frame_pending = 1;
            }
            audio_service();
        }
    }
#endif
}
