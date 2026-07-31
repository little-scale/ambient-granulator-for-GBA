#include "dsp.h"

#include "audio_controls.h"
#include "grain_position.h"
#include "grain_timing.h"

#include <string.h>

#if defined(__arm__)
#include <gba_base.h>
#else
#define IWRAM_CODE
#endif

static const uint32_t pitch_steps_q16[51] = {
    15464, 16384, 17360, 18392, 19484, 20644, 21872, 23172, 24548, 26008,
    27556, 29192, 30928, 32768, 34716, 36780, 38968, 41288, 43740,
    46340, 49096, 52016, 55108, 58384, 61856, 65536, 69432, 73560,
    77936, 82572, 87484, 92680, 98192, 104032, 110216, 116772,
    123716, 131072, 138864, 147124, 155872, 165140, 174968, 185364,
    196388, 208064, 220436, 233544, 247432, 262144, 277732
};

enum {
    /* Two milliseconds is long enough to de-click a stolen PCM8 voice. */
    STEAL_FADE_SAMPLES = 32
};

static int clamp16(int value)
{
    if ((uint32_t)value + 32768u <= 65535u)
        return value;
    return value < 0 ? -32768 : 32767;
}

static uint32_t next_random(DspState *state)
{
    uint32_t value = state->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state->random_state = value;
    return value;
}

static uint32_t random_bounded(DspState *state, uint32_t bound)
{
    return (uint32_t)(((uint64_t)next_random(state) * bound) >> 32);
}

static int random_signed(DspState *state, int amplitude)
{
    if (amplitude <= 0)
        return 0;
    return (int)random_bounded(state, (uint32_t)(amplitude * 2 + 1))
        - amplitude;
}

static int random_range_offset(DspState *state, int range)
{
    if (range >= 0)
        return (int)random_bounded(state, (uint32_t)(range * 2 + 1)) - range;
    return (int)random_bounded(state, (uint32_t)(-range + 1));
}

static inline void start_steal_tail(DspGrainVoice *voice)
    __attribute__((always_inline));
static inline void start_steal_tail(DspGrainVoice *voice)
{
    DspGrainTail *tail;
    int sample_index;

    if (!voice->active)
        return;
    sample_index = (int)(voice->position_q16 >> 16);
    if (sample_index >= voice->length)
        return;
    tail = &voice->tail;
    tail->source = voice->source;
    tail->position_q16 = voice->position_q16;
    tail->step_q16 = voice->step_q16;
    tail->length = voice->length;
    tail->left_gain_q8_12 = voice->left_envelope_gain_q8_12;
    tail->right_gain_q8_12 = voice->right_envelope_gain_q8_12;
    tail->left_gain_step_q8_12 = (tail->left_gain_q8_12
        + STEAL_FADE_SAMPLES - 2) / (STEAL_FADE_SAMPLES - 1);
    tail->right_gain_step_q8_12 = (tail->right_gain_q8_12
        + STEAL_FADE_SAMPLES - 2) / (STEAL_FADE_SAMPLES - 1);
    tail->remaining = STEAL_FADE_SAMPLES;
}

static int lowpass_alpha_q15(int cutoff_hz)
{
    int64_t omega_q15 = ((int64_t)cutoff_hz * 205887) / DSP_SAMPLE_RATE;
    return (int)((omega_q15 << 15) / (32768 + omega_q15));
}

static int highpass_alpha_q15(int cutoff_hz)
{
    int64_t omega_q15 = ((int64_t)cutoff_hz * 205887) / DSP_SAMPLE_RATE;
    return (int)(((int64_t)32768 << 15) / (32768 + omega_q15));
}

static void configure_grains(DspState *state)
{
    int length = state->parameters.value[PARAM_LENGTH]
        * DSP_SAMPLE_RATE / 1000;
    int release_samples;
    int base;

    if (length > DSP_MAX_GRAIN_SAMPLES)
        length = DSP_MAX_GRAIN_SAMPLES;
    if (state->sample != NULL && (uint32_t)length > state->sample_length)
        length = (int)state->sample_length;
    state->grain_length_samples = length;
    state->grain_attack_samples = length
        * state->parameters.value[PARAM_ATTACK] / 100;
    release_samples = length * state->parameters.value[PARAM_RELEASE] / 100;
    state->grain_release_start = length - release_samples;
    state->grain_attack_recip_q20 = state->grain_attack_samples > 0
        ? (1u << 20) / (uint32_t)state->grain_attack_samples : 0;
    state->grain_release_recip_q20 = release_samples > 0
        ? (1u << 20) / (uint32_t)release_samples : 0;
    base = grain_interval_samples(
        DSP_SAMPLE_RATE,
        state->parameters.value[PARAM_CLOCK],
        state->parameters.value[PARAM_BPM],
        parameter_divisions[state->parameters.value[PARAM_DIVISION]],
        state->parameters.value[PARAM_INTERVAL]);
    state->grain_interval_samples = base;
    state->grain_jitter_samples = base
        * state->parameters.value[PARAM_JITTER] / 100;
}

static void configure(DspState *state)
{
    static const int base_length[4] = { 421, 613, 809, 1013 };
    int scale = 55 + state->parameters.value[PARAM_REVERB_SIZE];
    int line;
    int highpass = state->parameters.value[PARAM_HIGHPASS];
    int lowpass = state->parameters.value[PARAM_LOWPASS];
    int highpass_was_enabled = state->highpass_enabled;
    int channel;

    for (line = 0; line < DSP_FDN_LINES; ++line) {
        int length = base_length[line] * scale / 100;
        if (length < 64)
            length = 64;
        if (length > DSP_FDN_MAX_DELAY)
            length = DSP_FDN_MAX_DELAY;
        state->reverb_length[line] = length;
        if (state->reverb_position[line] >= length)
            state->reverb_position[line] = 0;
    }
    state->reverb_freeze = state->parameters.value[PARAM_REVERB_FREEZE] != 0;
    state->reverb_feedback_q15 = state->reverb_freeze ? 32768
        : state->parameters.value[PARAM_REVERB_FEEDBACK] * 32768 / 1000;
    state->reverb_damp_q15 = 32767
        - state->parameters.value[PARAM_REVERB_DAMP] * 260;
    state->reverb_wet = state->parameters.value[PARAM_REVERB];
    state->reverb_wet_q8 = state->reverb_wet * 256 / 100;
    state->reverb_dry_q8 = 256 - state->reverb_wet_q8;
    state->highpass_enabled = highpass > 0;
    state->lowpass_enabled = lowpass < 8000;
    if (!highpass_was_enabled && state->highpass_enabled) {
        for (channel = 0; channel < 2; ++channel) {
            state->filter[channel].highpass_previous_input
                = state->filter[channel].lowpass_output;
            state->filter[channel].highpass_output = 0;
        }
    }
    state->highpass_alpha_q15 = highpass_alpha_q15(highpass > 0 ? highpass : 1);
    state->lowpass_alpha_q15 = lowpass_alpha_q15(lowpass);
    configure_grains(state);
}

static void queue_marker(DspState *state, int x)
{
    uint8_t next = (uint8_t)((state->marker_write + 1) % DSP_MARKER_QUEUE_SIZE);
    if (next == state->marker_read)
        return;
    state->marker_queue[state->marker_write] = (uint8_t)x;
    state->marker_write = next;
}

static void launch_grain(DspState *state, int x, int pitch, int fine)
    IWRAM_CODE __attribute__((noinline));
static void launch_grain(DspState *state, int x, int pitch, int fine)
{
    DspGrainVoice *voice;
    int length;
    int start;
    int pan;
    int left_gain;
    int right_gain;
    int gain_q12;
    uint32_t attack_fraction_q20;
    uint32_t release_fraction_q20;

    if (state->sample == NULL || state->sample_length == 0)
        return;
    length = state->grain_length_samples;
    start = x * ((int)state->sample_length - 1) / 239;
    if ((uint32_t)(start + length) > state->sample_length)
        start = (int)state->sample_length - length;
    if (pitch < -24)
        pitch = -24;
    if (pitch > 24)
        pitch = 24;
    if (fine < -100)
        fine = -100;
    if (fine > 100)
        fine = 100;

    pan = state->parameters.value[PARAM_PAN]
        + random_signed(state, state->parameters.value[PARAM_PAN_DEVIATION]);
    if (pan < -100)
        pan = -100;
    if (pan > 100)
        pan = 100;
    left_gain = state->parameters.value[PARAM_VOLUME]
        * audio_pan_left_percent(pan) * 256 / 10000;
    right_gain = state->parameters.value[PARAM_VOLUME]
        * audio_pan_right_percent(pan) * 256 / 10000;
    gain_q12 = audio_db_gain_q12(state->parameters.value[PARAM_GAIN]);

    voice = &state->voices[state->next_voice];
    state->next_voice = (state->next_voice + 1) % DSP_GRAIN_VOICES;
    start_steal_tail(voice);
    voice->active = 0;
    voice->source = state->sample + start;
    voice->position_q16 = 0;
    voice->step_q16 = dsp_pitch_step_cents_q16(pitch * 100 + fine);
    voice->length = length;
    voice->attack_samples = state->grain_attack_samples;
    voice->release_start = state->grain_release_start;
    voice->left_gain_q8 = left_gain * gain_q12 / 4096;
    voice->right_gain_q8 = right_gain * gain_q12 / 4096;
    voice->left_envelope_gain_q8_12 = voice->attack_samples > 0
        ? 0 : voice->left_gain_q8 << 12;
    voice->right_envelope_gain_q8_12 = voice->attack_samples > 0
        ? 0 : voice->right_gain_q8 << 12;
    attack_fraction_q20 = (uint32_t)(
        (uint64_t)voice->step_q16 * state->grain_attack_recip_q20 >> 16);
    release_fraction_q20 = (uint32_t)(
        (uint64_t)voice->step_q16 * state->grain_release_recip_q20 >> 16);
    if (voice->attack_samples > 0) {
        voice->left_attack_step_q8_12 = (int)(
            (uint32_t)voice->left_gain_q8 * attack_fraction_q20 >> 8);
        voice->right_attack_step_q8_12 = (int)(
            (uint32_t)voice->right_gain_q8 * attack_fraction_q20 >> 8);
    } else {
        voice->left_attack_step_q8_12 = 0;
        voice->right_attack_step_q8_12 = 0;
    }
    {
        int release_samples = voice->length - voice->release_start;
        if (release_samples > 0) {
            voice->left_release_step_q8_12 = (int)(
                (uint32_t)voice->left_gain_q8 * release_fraction_q20 >> 8);
            voice->right_release_step_q8_12 = (int)(
                (uint32_t)voice->right_gain_q8 * release_fraction_q20 >> 8);
        } else {
            voice->left_release_step_q8_12 = 0;
            voice->right_release_step_q8_12 = 0;
        }
    }
    voice->active = 1;
    ++state->grains_started;
    queue_marker(state, x);
}

static void schedule_grain(DspState *state)
    IWRAM_CODE __attribute__((noinline));
static void schedule_grain(DspState *state)
{
    int range;
    int x;
    int pitch;
    int fine;
    int base;
    int jitter;
    int delay;

    range = state->parameters.value[PARAM_RANGE];
    x = state->burst_center + random_range_offset(state, range);
    if (x < 0)
        x = 0;
    if (x > 239)
        x = 239;
    pitch = state->parameters.value[PARAM_PITCH]
        + random_signed(state, state->parameters.value[PARAM_PITCH_DEVIATION]);
    fine = state->parameters.value[PARAM_FINE]
        + random_signed(state, state->parameters.value[PARAM_FINE_DEVIATION]);
    launch_grain(state, x, pitch, fine);
    --state->burst_remaining;
    if (state->burst_remaining <= 0 && state->gate_active)
        state->burst_remaining = state->parameters.value[PARAM_GRAINS];
    if (state->burst_remaining <= 0)
        return;

    base = state->grain_interval_samples;
    jitter = state->grain_jitter_samples;
    delay = base + random_signed(state, jitter);
    state->samples_until_grain = delay < 1 ? 1 : delay;
}

/*
 * Mix a run with no scheduler event.  Keeping each voice's position and
 * envelope fields in registers across the run is much cheaper on ARM7TDMI
 * than reloading all four voice structures for every output sample.
 */
static void mix_grain_voice_span(DspGrainVoice *restrict voice,
                                 int32_t *restrict dry_left,
                                 int32_t *restrict dry_right, int count)
    IWRAM_CODE __attribute__((noinline));
static void mix_grain_voice_span(DspGrainVoice *restrict voice,
                                 int32_t *restrict dry_left,
                                 int32_t *restrict dry_right, int count)
{
    DspGrainTail *tail = &voice->tail;
    int frame;

    if (tail->remaining > 0) {
        uint32_t position_q16 = tail->position_q16;
        uint32_t step_q16 = tail->step_q16;
        const int8_t *source = tail->source;
        int length = tail->length;
        int left_gain_q8_12 = tail->left_gain_q8_12;
        int right_gain_q8_12 = tail->right_gain_q8_12;
        int left_gain_step_q8_12 = tail->left_gain_step_q8_12;
        int right_gain_step_q8_12 = tail->right_gain_step_q8_12;
        int remaining = tail->remaining;
        for (frame = 0; frame < count && remaining > 0; ++frame) {
            int tail_sample_index = (int)(position_q16 >> 16);
            if (tail_sample_index >= length) {
                remaining = 0;
                break;
            }
            {
                int tail_sample = source[tail_sample_index];
                dry_left[frame] += tail_sample * left_gain_q8_12 >> 12;
                dry_right[frame] += tail_sample * right_gain_q8_12 >> 12;
            }
            left_gain_q8_12 -= left_gain_step_q8_12;
            right_gain_q8_12 -= right_gain_step_q8_12;
            if (left_gain_q8_12 < 0)
                left_gain_q8_12 = 0;
            if (right_gain_q8_12 < 0)
                right_gain_q8_12 = 0;
            position_q16 += step_q16;
            --remaining;
        }
        tail->position_q16 = position_q16;
        tail->left_gain_q8_12 = left_gain_q8_12;
        tail->right_gain_q8_12 = right_gain_q8_12;
        tail->remaining = remaining;
    }

    if (voice->active) {
        uint32_t position_q16 = voice->position_q16;
        uint32_t step_q16 = voice->step_q16;
        const int8_t *source = voice->source;
        int length = voice->length;
        int attack_samples = voice->attack_samples;
        int release_start = voice->release_start;
        int left_gain_q8 = voice->left_gain_q8;
        int right_gain_q8 = voice->right_gain_q8;
        int left_envelope_gain_q8_12 = voice->left_envelope_gain_q8_12;
        int right_envelope_gain_q8_12 = voice->right_envelope_gain_q8_12;
        int left_attack_step_q8_12 = voice->left_attack_step_q8_12;
        int right_attack_step_q8_12 = voice->right_attack_step_q8_12;
        int left_release_step_q8_12 = voice->left_release_step_q8_12;
        int right_release_step_q8_12 = voice->right_release_step_q8_12;
        frame = 0;

        /* Attack, sustain and release use separate hot loops. */
        while (frame < count && attack_samples > 0) {
            int sample_index = (int)(position_q16 >> 16);
            int sample;
            if (sample_index >= length) {
                voice->active = 0;
                goto voice_done;
            }
            if (sample_index >= attack_samples)
                break;
            sample = source[sample_index];
            dry_left[frame] += sample * left_envelope_gain_q8_12 >> 12;
            dry_right[frame] += sample * right_envelope_gain_q8_12 >> 12;
            left_envelope_gain_q8_12 += left_attack_step_q8_12;
            right_envelope_gain_q8_12 += right_attack_step_q8_12;
            position_q16 += step_q16;
            ++frame;
        }

        if (frame < count
                && (int)(position_q16 >> 16) < release_start) {
            left_envelope_gain_q8_12 = left_gain_q8 << 12;
            right_envelope_gain_q8_12 = right_gain_q8 << 12;
            while (frame < count) {
                int sample_index = (int)(position_q16 >> 16);
                int sample;
                if (sample_index >= length) {
                    voice->active = 0;
                    goto voice_done;
                }
                if (sample_index >= release_start)
                    break;
                sample = source[sample_index];
                dry_left[frame] += sample * left_envelope_gain_q8_12 >> 12;
                dry_right[frame] += sample * right_envelope_gain_q8_12 >> 12;
                position_q16 += step_q16;
                ++frame;
            }
        }

        while (frame < count) {
            int sample_index = (int)(position_q16 >> 16);
            int sample;
            if (sample_index >= length) {
                voice->active = 0;
                break;
            }
            sample = source[sample_index];
            dry_left[frame] += sample * left_envelope_gain_q8_12 >> 12;
            dry_right[frame] += sample * right_envelope_gain_q8_12 >> 12;
            left_envelope_gain_q8_12 -= left_release_step_q8_12;
            right_envelope_gain_q8_12 -= right_release_step_q8_12;
            if (left_envelope_gain_q8_12 < 0)
                left_envelope_gain_q8_12 = 0;
            if (right_envelope_gain_q8_12 < 0)
                right_envelope_gain_q8_12 = 0;
            position_q16 += step_q16;
            ++frame;
        }
voice_done:
        voice->position_q16 = position_q16;
        voice->left_envelope_gain_q8_12 = left_envelope_gain_q8_12;
        voice->right_envelope_gain_q8_12 = right_envelope_gain_q8_12;
    }
}

static void filter_channel_block(DspFilterState *filter,
                                 const int16_t *restrict input,
                                 int8_t *restrict output, int count,
                                 int highpass_enabled, int highpass_alpha,
                                 int lowpass_enabled, int lowpass_alpha)
    IWRAM_CODE __attribute__((noinline));
static void filter_channel_block(DspFilterState *filter,
                                 const int16_t *restrict input,
                                 int8_t *restrict output, int count,
                                 int highpass_enabled, int highpass_alpha,
                                 int lowpass_enabled, int lowpass_alpha)
{
    int previous_input = filter->highpass_previous_input;
    int highpass_output = filter->highpass_output;
    int lowpass_output = filter->lowpass_output;
    int frame;

    if (highpass_enabled && lowpass_enabled) {
        for (frame = 0; frame < count; ++frame) {
            int value = input[frame];
            int difference = highpass_output + value - previous_input;
            if (difference < -65536)
                difference = -65536;
            if (difference > 65535)
                difference = 65535;
            highpass_output = difference * highpass_alpha >> 15;
            previous_input = value;
            lowpass_output += (highpass_output - lowpass_output)
                * lowpass_alpha >> 15;
            output[frame] = (int8_t)(clamp16(lowpass_output) >> 8);
        }
    } else if (highpass_enabled) {
        for (frame = 0; frame < count; ++frame) {
            int value = input[frame];
            int difference = highpass_output + value - previous_input;
            if (difference < -65536)
                difference = -65536;
            if (difference > 65535)
                difference = 65535;
            highpass_output = difference * highpass_alpha >> 15;
            previous_input = value;
            lowpass_output = highpass_output;
            output[frame] = (int8_t)(clamp16(highpass_output) >> 8);
        }
    } else {
        highpass_output = 0;
        for (frame = 0; frame < count; ++frame) {
            int value = input[frame];
            previous_input = value;
            lowpass_output += (value - lowpass_output) * lowpass_alpha >> 15;
            output[frame] = (int8_t)(clamp16(lowpass_output) >> 8);
        }
    }
    filter->highpass_previous_input = previous_input;
    filter->highpass_output = highpass_output;
    filter->lowpass_output = lowpass_output;
}

void dsp_init(DspState *state, DspReverbMemory *reverb_memory)
{
    int line;
    memset(state, 0, sizeof(*state));
    memset(reverb_memory, 0, sizeof(*reverb_memory));
    state->reverb_memory = reverb_memory;
    for (line = 0; line < DSP_FDN_LINES; ++line)
        state->reverb_delay[line] = reverb_memory->delay[line];
    state->random_state = 0x6d2b79f5u;
    parameters_reset(&state->parameters);
    configure(state);
}

void dsp_set_reverb_line(DspState *state, int line, int16_t *delay)
{
    if ((unsigned int)line < DSP_FDN_LINES && delay != NULL)
        state->reverb_delay[line] = delay;
}

void dsp_seed_random(DspState *state, uint32_t seed)
{
    state->random_state = seed != 0 ? seed : 0x6d2b79f5u;
}

void dsp_set_parameters(DspState *state, const ParameterState *parameters)
{
    state->parameters = *parameters;
    configure(state);
}

void dsp_set_sample(DspState *state, const int8_t *sample, uint32_t length)
{
    int index;
    state->sample = sample;
    state->sample_length = length;
    configure_grains(state);
    state->burst_remaining = 0;
    state->gate_active = 0;
    for (index = 0; index < DSP_GRAIN_VOICES; ++index) {
        start_steal_tail(&state->voices[index]);
        state->voices[index].active = 0;
    }
}

void dsp_trigger_burst(DspState *state, int center_x)
{
    dsp_trigger_burst_count(
        state, center_x, state->parameters.value[PARAM_GRAINS]);
}

void dsp_trigger_burst_count(DspState *state, int center_x, int count)
{
    if (center_x < 0)
        center_x = 0;
    if (center_x > 239)
        center_x = 239;
    if (count < 1) {
        state->burst_remaining = 0;
        state->samples_until_grain = 0;
        return;
    }
    if (count > 32)
        count = 32;
    state->burst_center = center_x;
    state->burst_remaining = count;
    state->samples_until_grain = 0;
    state->random_state ^= ((uint32_t)center_x << 16) ^ state->grains_started;
}

void dsp_set_gate(DspState *state, int active, int center_x)
{
    state->gate_center = center_x < 0 ? 0 : (center_x > 239 ? 239 : center_x);
    if (active && !state->gate_active) {
        state->gate_active = 1;
        dsp_trigger_burst(state, state->gate_center);
    } else if (!active) {
        state->gate_active = 0;
    }
}

void dsp_render(DspState *restrict state, int8_t *restrict left,
                int8_t *restrict right, int count) IWRAM_CODE;
void dsp_render(DspState *restrict state, int8_t *restrict left,
                int8_t *restrict right, int count)
{
    int frame;
    int output_frame;
    int active_voice = 0;
    int reverb_activity = state->reverb_active;
    int16_t *restrict reverb_delay0 = state->reverb_delay[0];
    int16_t *restrict reverb_delay1 = state->reverb_delay[1];
    int16_t *restrict reverb_delay2 = state->reverb_delay[2];
    int16_t *restrict reverb_delay3 = state->reverb_delay[3];
    int reverb_length0 = state->reverb_length[0];
    int reverb_length1 = state->reverb_length[1];
    int reverb_length2 = state->reverb_length[2];
    int reverb_length3 = state->reverb_length[3];
    int reverb_position0 = state->reverb_position[0];
    int reverb_position1 = state->reverb_position[1];
    int reverb_position2 = state->reverb_position[2];
    int reverb_position3 = state->reverb_position[3];
    int reverb_damped0 = state->reverb_damped[0];
    int reverb_damped1 = state->reverb_damped[1];
    int reverb_damped2 = state->reverb_damped[2];
    int reverb_damped3 = state->reverb_damped[3];
    int reverb_feedback_q15 = state->reverb_feedback_q15;
    int reverb_damp_q15 = state->reverb_damp_q15;
    int reverb_wet_q8 = state->reverb_wet_q8;
    int reverb_freeze = state->reverb_freeze;
    int filters_enabled = state->highpass_enabled || state->lowpass_enabled;
    int bypass_left = state->filter[0].lowpass_output;
    int bypass_right = state->filter[1].lowpass_output;

    for (frame = 0; frame < DSP_GRAIN_VOICES; ++frame)
        active_voice |= state->voices[frame].active
                     | (state->voices[frame].tail.remaining > 0);
    if (!active_voice && state->burst_remaining <= 0 && !state->gate_active
            && !state->reverb_active
            && state->filter[0].highpass_output == 0
            && state->filter[0].lowpass_output == 0
            && state->filter[1].highpass_output == 0
            && state->filter[1].lowpass_output == 0) {
        memset(left, 0, (size_t)count);
        memset(right, 0, (size_t)count);
        return;
    }

    for (output_frame = 0; output_frame < count;) {
        int chunk_count = count - output_frame;
        if (chunk_count > DSP_FILTER_CHUNK_SAMPLES)
            chunk_count = DSP_FILTER_CHUNK_SAMPLES;
#ifndef AMBIENT_PROFILE_EFFECTS_ONLY
        for (frame = 0; frame < chunk_count; ++frame) {
            state->grain_left_scratch[frame] = 0;
            state->grain_right_scratch[frame] = 0;
        }
        {
            int grain_frame = 0;
            while (grain_frame < chunk_count) {
                int span = chunk_count - grain_frame;
                int voice_index;

                /* Emulate one scheduler tick, then batch the event-free run. */
                if (state->gate_active)
                    state->burst_center = state->gate_center;
                if (state->burst_remaining > 0) {
                    if (state->samples_until_grain > 0)
                        --state->samples_until_grain;
                    if (state->samples_until_grain <= 0)
                        schedule_grain(state);
                    if (state->burst_remaining > 0
                            && state->samples_until_grain > 0
                            && span > state->samples_until_grain) {
                        span = state->samples_until_grain;
                    }
                }
                for (voice_index = 0;
                     voice_index < DSP_GRAIN_VOICES; ++voice_index) {
                    mix_grain_voice_span(
                        &state->voices[voice_index],
                        state->grain_left_scratch + grain_frame,
                        state->grain_right_scratch + grain_frame, span);
                }
                if (state->burst_remaining > 0 && span > 1)
                    state->samples_until_grain -= span - 1;
                grain_frame += span;
            }
        }
#endif
        for (frame = 0; frame < chunk_count; ++frame) {
#ifdef AMBIENT_PROFILE_EFFECTS_ONLY
        int dry_left = 0;
        int dry_right = 0;
#else
        int dry_left = state->grain_left_scratch[frame];
        int dry_right = state->grain_right_scratch[frame];
#endif
        int input0;
        int input1;
        int input2;
        int input3;
        int delayed0;
        int delayed1;
        int delayed2;
        int delayed3;
        int sum01;
        int difference01;
        int sum23;
        int difference23;
        int target0;
        int target1;
        int target2;
        int target3;
        int wet_left;
        int wet_right;
        int mixed_left;
        int mixed_right;

#ifdef AMBIENT_PROFILE_GRAINS_ONLY
        mixed_left = clamp16(dry_left);
        mixed_right = clamp16(dry_right);
        left[output_frame + frame] = (int8_t)(mixed_left >> 8);
        right[output_frame + frame] = (int8_t)(mixed_right >> 8);
        continue;
#endif

        if (!reverb_freeze) {
            int send_left = clamp16(dry_left);
            int send_right = clamp16(dry_right);
            input0 = (send_left + send_right) >> 2;
            input1 = (send_left - send_right) >> 2;
            input2 = input0;
            input3 = (-send_left + send_right) >> 2;
            reverb_activity |= dry_left | dry_right;
        } else {
            input0 = 0;
            input1 = 0;
            input2 = 0;
            input3 = 0;
        }

        delayed0 = reverb_delay0[reverb_position0];
        delayed1 = reverb_delay1[reverb_position1];
        delayed2 = reverb_delay2[reverb_position2];
        delayed3 = reverb_delay3[reverb_position3];
        sum01 = delayed0 + delayed1;
        difference01 = delayed0 - delayed1;
        sum23 = delayed2 + delayed3;
        difference23 = delayed2 - delayed3;
        target0 = clamp16((sum01 + sum23) >> 1);
        target1 = clamp16((difference01 + difference23) >> 1);
        target2 = clamp16((sum01 - sum23) >> 1);
        target3 = clamp16((difference01 - difference23) >> 1);

        if (reverb_freeze) {
            reverb_damped0 = target0;
            reverb_damped1 = target1;
            reverb_damped2 = target2;
            reverb_damped3 = target3;
            reverb_delay0[reverb_position0]
                = (int16_t)clamp16(input0 + target0);
            reverb_delay1[reverb_position1]
                = (int16_t)clamp16(input1 + target1);
            reverb_delay2[reverb_position2]
                = (int16_t)clamp16(input2 + target2);
            reverb_delay3[reverb_position3]
                = (int16_t)clamp16(input3 + target3);
        } else {
            int feedback0;
            int feedback1;
            int feedback2;
            int feedback3;
            reverb_damped0 += (target0 - reverb_damped0)
                * reverb_damp_q15 >> 15;
            reverb_damped1 += (target1 - reverb_damped1)
                * reverb_damp_q15 >> 15;
            reverb_damped2 += (target2 - reverb_damped2)
                * reverb_damp_q15 >> 15;
            reverb_damped3 += (target3 - reverb_damped3)
                * reverb_damp_q15 >> 15;
            feedback0 = reverb_damped0 * reverb_feedback_q15 >> 15;
            feedback1 = reverb_damped1 * reverb_feedback_q15 >> 15;
            feedback2 = reverb_damped2 * reverb_feedback_q15 >> 15;
            feedback3 = reverb_damped3 * reverb_feedback_q15 >> 15;
            reverb_delay0[reverb_position0]
                = (int16_t)clamp16(input0 + feedback0);
            reverb_delay1[reverb_position1]
                = (int16_t)clamp16(input1 + feedback1);
            reverb_delay2[reverb_position2]
                = (int16_t)clamp16(input2 + feedback2);
            reverb_delay3[reverb_position3]
                = (int16_t)clamp16(input3 + feedback3);
        }

        if (++reverb_position0 >= reverb_length0)
            reverb_position0 = 0;
        if (++reverb_position1 >= reverb_length1)
            reverb_position1 = 0;
        if (++reverb_position2 >= reverb_length2)
            reverb_position2 = 0;
        if (++reverb_position3 >= reverb_length3)
            reverb_position3 = 0;
        wet_left = (sum01 + difference23) >> 2;
        wet_right = (difference01 + sum23) >> 2;
        if (reverb_wet_q8 >= 256) {
            mixed_left = wet_left;
            mixed_right = wet_right;
        } else if (reverb_wet_q8 <= 0) {
            mixed_left = dry_left;
            mixed_right = dry_right;
        } else {
            mixed_left = dry_left
                + ((wet_left - dry_left) * reverb_wet_q8 >> 8);
            mixed_right = dry_right
                + ((wet_right - dry_right) * reverb_wet_q8 >> 8);
        }
        if (filters_enabled) {
            state->filter_left_scratch[frame] = (int16_t)clamp16(mixed_left);
            state->filter_right_scratch[frame] = (int16_t)clamp16(mixed_right);
        } else {
            mixed_left = clamp16(mixed_left);
            mixed_right = clamp16(mixed_right);
            bypass_left = mixed_left;
            bypass_right = mixed_right;
        }
        if (!filters_enabled) {
            left[output_frame + frame] = (int8_t)(mixed_left >> 8);
            right[output_frame + frame] = (int8_t)(mixed_right >> 8);
        }
        }
        if (filters_enabled) {
            filter_channel_block(
                &state->filter[0], state->filter_left_scratch,
                left + output_frame, chunk_count,
                state->highpass_enabled, state->highpass_alpha_q15,
                state->lowpass_enabled, state->lowpass_alpha_q15);
            filter_channel_block(
                &state->filter[1], state->filter_right_scratch,
                right + output_frame, chunk_count,
                state->highpass_enabled, state->highpass_alpha_q15,
                state->lowpass_enabled, state->lowpass_alpha_q15);
        }
        output_frame += chunk_count;
    }
    state->reverb_position[0] = reverb_position0;
    state->reverb_position[1] = reverb_position1;
    state->reverb_position[2] = reverb_position2;
    state->reverb_position[3] = reverb_position3;
    state->reverb_damped[0] = reverb_damped0;
    state->reverb_damped[1] = reverb_damped1;
    state->reverb_damped[2] = reverb_damped2;
    state->reverb_damped[3] = reverb_damped3;
    if (!filters_enabled) {
        state->filter[0].lowpass_output = bypass_left;
        state->filter[1].lowpass_output = bypass_right;
    }
    state->reverb_active = reverb_activity != 0;
}

int dsp_pop_marker(DspState *state, uint8_t *x)
{
    if (state->marker_read == state->marker_write)
        return 0;
    *x = state->marker_queue[state->marker_read];
    state->marker_read = (uint8_t)((state->marker_read + 1)
        % DSP_MARKER_QUEUE_SIZE);
    return 1;
}

uint32_t dsp_pitch_step_q16(int semitones)
{
    if (semitones < -24)
        semitones = -24;
    if (semitones > 24)
        semitones = 24;
    return pitch_steps_q16[semitones + 25];
}

uint32_t dsp_pitch_step_cents_q16(int total_cents)
{
    int lower;
    int remainder;
    uint32_t first;
    uint32_t second;

    if (total_cents < -2500)
        total_cents = -2500;
    if (total_cents > 2500)
        total_cents = 2500;
    if (total_cents == 2500)
        return pitch_steps_q16[50];
    lower = total_cents / 100;
    remainder = total_cents % 100;
    if (remainder < 0) {
        --lower;
        remainder += 100;
    }
    first = pitch_steps_q16[lower + 25];
    second = pitch_steps_q16[lower + 26];
    return first + (second - first) * (uint32_t)remainder / 100u;
}
