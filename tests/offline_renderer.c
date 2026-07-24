#include "dsp.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BLOCK = 256 };

typedef enum {
    RENDER_DEFAULT,
    RENDER_FREEZE,
    RENDER_LEFT,
    RENDER_RIGHT,
    RENDER_MAXIMUM,
    RENDER_IMPULSE_REVERB,
    RENDER_IMPULSE_FREEZE,
    RENDER_REPEATED_FREEZE,
    RENDER_HIGHPASS_STEP,
    RENDER_LOWPASS_IMPULSE,
    RENDER_SINGLE_GRAIN,
    RENDER_MAXIMUM_FEEDBACK,
    RENDER_MAXIMUM_GAIN
} RenderScenario;

static DspState state;
static DspReverbMemory reverb;
static int8_t left[BLOCK];
static int8_t right[BLOCK];

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_u16(FILE *file, uint16_t value)
{
    fputc((int)(value & 0xff), file);
    fputc((int)(value >> 8), file);
}

static void write_u32(FILE *file, uint32_t value)
{
    write_u16(file, (uint16_t)(value & 0xffff));
    write_u16(file, (uint16_t)(value >> 16));
}

static void write_wav_header(FILE *file, int frames)
{
    uint32_t bytes = (uint32_t)frames * 4;
    fwrite("RIFF", 1, 4, file);
    write_u32(file, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, file);
    write_u32(file, 16);
    write_u16(file, 1);
    write_u16(file, 2);
    write_u32(file, DSP_SAMPLE_RATE);
    write_u32(file, DSP_SAMPLE_RATE * 4);
    write_u16(file, 4);
    write_u16(file, 16);
    fwrite("data", 1, 4, file);
    write_u32(file, bytes);
}

static int active_voices(const DspState *dsp)
{
    int count = 0;
    int voice;
    for (voice = 0; voice < DSP_GRAIN_VOICES; ++voice)
        count += dsp->voices[voice].active != 0;
    return count;
}

static void configure_scenario(ParameterState *parameters,
                               RenderScenario scenario)
{
    parameters_reset(parameters);
    if (scenario == RENDER_IMPULSE_REVERB
            || scenario == RENDER_IMPULSE_FREEZE
            || scenario == RENDER_REPEATED_FREEZE
            || scenario == RENDER_MAXIMUM_FEEDBACK) {
        parameters->value[PARAM_PITCH_DEVIATION] = 0;
        parameters->value[PARAM_FINE_DEVIATION] = 0;
        parameters->value[PARAM_GRAINS] = 1;
        parameters->value[PARAM_LENGTH] = 500;
        parameters->value[PARAM_ATTACK] = 0;
        parameters->value[PARAM_RELEASE] = 0;
        parameters->value[PARAM_PAN] = -100;
        parameters->value[PARAM_PAN_DEVIATION] = 0;
        parameters->value[PARAM_REVERB] = 100;
        parameters->value[PARAM_REVERB_FEEDBACK]
            = scenario == RENDER_MAXIMUM_FEEDBACK ? 999 : 900;
        parameters->value[PARAM_REVERB_SIZE] = 100;
        parameters->value[PARAM_REVERB_DAMP] = 10;
        parameters->value[PARAM_HIGHPASS] = 0;
        parameters->value[PARAM_LOWPASS] = 8000;
    } else if (scenario == RENDER_HIGHPASS_STEP
            || scenario == RENDER_LOWPASS_IMPULSE) {
        parameters->value[PARAM_PITCH_DEVIATION] = 0;
        parameters->value[PARAM_FINE_DEVIATION] = 0;
        parameters->value[PARAM_GRAINS] = 1;
        parameters->value[PARAM_LENGTH] = 500;
        parameters->value[PARAM_ATTACK] = 0;
        parameters->value[PARAM_RELEASE] = 0;
        parameters->value[PARAM_PAN_DEVIATION] = 0;
        parameters->value[PARAM_REVERB] = 0;
        parameters->value[PARAM_HIGHPASS]
            = scenario == RENDER_HIGHPASS_STEP ? 1000 : 0;
        parameters->value[PARAM_LOWPASS]
            = scenario == RENDER_LOWPASS_IMPULSE ? 1000 : 8000;
    } else if (scenario == RENDER_SINGLE_GRAIN) {
        parameters->value[PARAM_RANGE] = 0;
        parameters->value[PARAM_PITCH] = 0;
        parameters->value[PARAM_FINE] = 0;
        parameters->value[PARAM_PITCH_DEVIATION] = 0;
        parameters->value[PARAM_FINE_DEVIATION] = 0;
        parameters->value[PARAM_GRAINS] = 1;
        parameters->value[PARAM_LENGTH] = 500;
        parameters->value[PARAM_ATTACK] = 50;
        parameters->value[PARAM_RELEASE] = 50;
        parameters->value[PARAM_PAN] = 0;
        parameters->value[PARAM_PAN_DEVIATION] = 0;
        parameters->value[PARAM_REVERB] = 0;
        parameters->value[PARAM_HIGHPASS] = 0;
        parameters->value[PARAM_LOWPASS] = 8000;
    } else if (scenario == RENDER_MAXIMUM_GAIN) {
        parameters->value[PARAM_RANGE] = 0;
        parameters->value[PARAM_PITCH] = 0;
        parameters->value[PARAM_FINE] = 0;
        parameters->value[PARAM_PITCH_DEVIATION] = 0;
        parameters->value[PARAM_FINE_DEVIATION] = 0;
        parameters->value[PARAM_GRAINS] = 1;
        parameters->value[PARAM_LENGTH] = 500;
        parameters->value[PARAM_ATTACK] = 0;
        parameters->value[PARAM_RELEASE] = 0;
        parameters->value[PARAM_GAIN] = 18;
        parameters->value[PARAM_PAN] = -100;
        parameters->value[PARAM_PAN_DEVIATION] = 0;
        parameters->value[PARAM_REVERB] = 0;
        parameters->value[PARAM_HIGHPASS] = 0;
        parameters->value[PARAM_LOWPASS] = 8000;
    } else if (scenario == RENDER_LEFT || scenario == RENDER_RIGHT) {
        parameters->value[PARAM_PAN] = scenario == RENDER_LEFT ? -100 : 100;
        parameters->value[PARAM_PAN_DEVIATION] = 0;
        parameters->value[PARAM_REVERB] = 100;
        parameters->value[PARAM_GRAINS] = 1;
    } else if (scenario == RENDER_MAXIMUM) {
        parameters->value[PARAM_PITCH] = 0;
        parameters->value[PARAM_FINE] = 0;
        parameters->value[PARAM_PITCH_DEVIATION] = 12;
        parameters->value[PARAM_FINE_DEVIATION] = 100;
        parameters->value[PARAM_CLOCK] = 0;
        parameters->value[PARAM_INTERVAL] = 20;
        parameters->value[PARAM_JITTER] = 0;
        parameters->value[PARAM_GRAINS] = 32;
        parameters->value[PARAM_LENGTH] = 500;
        parameters->value[PARAM_ATTACK] = 10;
        parameters->value[PARAM_RELEASE] = 10;
        parameters->value[PARAM_GAIN] = 0;
        parameters->value[PARAM_VOLUME] = 100;
        parameters->value[PARAM_PAN] = 0;
        parameters->value[PARAM_PAN_DEVIATION] = 100;
        parameters->value[PARAM_REVERB] = 100;
        parameters->value[PARAM_REVERB_FEEDBACK] = 990;
        parameters->value[PARAM_REVERB_DAMP] = 10;
        parameters->value[PARAM_REVERB_SIZE] = 100;
        parameters->value[PARAM_HIGHPASS] = 0;
        parameters->value[PARAM_LOWPASS] = 2000;
    }
}

static void render_file(const char *path, const int8_t *sample,
                        uint32_t sample_length, RenderScenario scenario,
                        int seconds)
{
    ParameterState parameters;
    FILE *file = fopen(path, "wb");
    int total_frames = seconds * DSP_SAMPLE_RATE;
    int frame = 0;
    int saw_audio = 0;
    int saw_stereo = 0;
    int saw_late_audio = 0;
    int maximum_voices = 0;
    int previous_left = 0;
    int previous_right = 0;
    int maximum_jump_left = 0;
    int maximum_jump_right = 0;
    int peak_left = 0;
    int peak_right = 0;

    assert(file != NULL);
    configure_scenario(&parameters, scenario);
    dsp_init(&state, &reverb);
    dsp_set_parameters(&state, &parameters);
    dsp_set_sample(&state, sample, sample_length);
    if (scenario >= RENDER_IMPULSE_REVERB)
        dsp_trigger_burst(&state, 0);
    else
        dsp_set_gate(&state, 1, 121);
    write_wav_header(file, total_frames);

    while (frame < total_frames) {
        int count = total_frames - frame > BLOCK ? BLOCK : total_frames - frame;
        int index;
        if (scenario == RENDER_DEFAULT && frame == 4 * DSP_SAMPLE_RATE)
            dsp_set_gate(&state, 0, 121);
        if (scenario == RENDER_FREEZE) {
            if (frame == DSP_SAMPLE_RATE)
                dsp_set_gate(&state, 0, 121);
            if (frame == 2 * DSP_SAMPLE_RATE) {
                parameters.value[PARAM_REVERB_FREEZE] = 1;
                dsp_set_parameters(&state, &parameters);
            }
            if (frame == 4 * DSP_SAMPLE_RATE)
                dsp_trigger_burst(&state, 160);
        }
        if (scenario == RENDER_IMPULSE_FREEZE
                && frame == DSP_SAMPLE_RATE / 2) {
            parameters.value[PARAM_REVERB_FREEZE] = 1;
            dsp_set_parameters(&state, &parameters);
        }
        if (scenario == RENDER_REPEATED_FREEZE
                && frame > 0
                && frame % (DSP_SAMPLE_RATE / 2) == 0) {
            parameters.value[PARAM_REVERB_FREEZE]
                = !parameters.value[PARAM_REVERB_FREEZE];
            dsp_set_parameters(&state, &parameters);
        }
        if ((scenario == RENDER_LEFT || scenario == RENDER_RIGHT)
                && frame == DSP_SAMPLE_RATE)
            dsp_set_gate(&state, 0, 121);
        dsp_render(&state, left, right, count);
        if (active_voices(&state) > maximum_voices)
            maximum_voices = active_voices(&state);
        for (index = 0; index < count; ++index) {
            int jump_left = abs((int)left[index] - previous_left);
            int jump_right = abs((int)right[index] - previous_right);
            int16_t output_left = (int16_t)((int)left[index] << 8);
            int16_t output_right = (int16_t)((int)right[index] << 8);
            if (jump_left > maximum_jump_left)
                maximum_jump_left = jump_left;
            if (jump_right > maximum_jump_right)
                maximum_jump_right = jump_right;
            if (abs((int)left[index]) > peak_left)
                peak_left = abs((int)left[index]);
            if (abs((int)right[index]) > peak_right)
                peak_right = abs((int)right[index]);
            previous_left = left[index];
            previous_right = right[index];
            write_u16(file, (uint16_t)output_left);
            write_u16(file, (uint16_t)output_right);
            saw_audio |= left[index] != 0 || right[index] != 0;
            saw_stereo |= left[index] != right[index];
            if (frame + index >= total_frames - DSP_SAMPLE_RATE / 2)
                saw_late_audio |= left[index] != 0 || right[index] != 0;
        }
        frame += count;
    }
    assert(fclose(file) == 0);
    assert(saw_audio);
    if (scenario == RENDER_LEFT || scenario == RENDER_RIGHT
            || scenario == RENDER_IMPULSE_REVERB
            || scenario == RENDER_IMPULSE_FREEZE
            || scenario == RENDER_REPEATED_FREEZE)
        assert(saw_stereo);
    if (scenario == RENDER_IMPULSE_FREEZE
            || scenario == RENDER_REPEATED_FREEZE
            || scenario == RENDER_MAXIMUM_FEEDBACK)
        assert(saw_late_audio);
    if (scenario == RENDER_SINGLE_GRAIN)
        assert(maximum_voices == 1 && state.grains_started == 1);
    if (scenario == RENDER_MAXIMUM_FEEDBACK) {
        assert(maximum_voices == 1 && state.grains_started == 1);
        assert(state.reverb_feedback_q15 == 32735);
        assert(peak_left <= 127 && peak_right <= 127);
    }
    if (scenario == RENDER_MAXIMUM_GAIN) {
        assert(maximum_voices == 1 && state.grains_started == 1);
        assert(peak_left == 127 && peak_right == 0);
    }
    if (scenario == RENDER_MAXIMUM) {
        assert(maximum_voices == DSP_GRAIN_VOICES);
        assert(state.reverb_feedback_q15 == 32440);
        printf("%s maximum-load PCM8 jump: L=%d R=%d\n", path,
               maximum_jump_left, maximum_jump_right);
        assert(maximum_jump_left <= 64 && maximum_jump_right <= 64);
    }
}

int main(void)
{
    FILE *file = fopen("assets/sample_bank.bin", "rb");
    uint8_t *bank;
    long size;
    uint32_t pcm_offset;
    uint32_t pcm_length;
    uint32_t second_pcm_offset;
    uint32_t second_pcm_length;
    static int8_t impulse[DSP_MAX_GRAIN_SAMPLES];
    static int8_t step[DSP_MAX_GRAIN_SAMPLES];

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    bank = malloc((size_t)size);
    assert(bank != NULL && fread(bank, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    pcm_offset = read_u32(bank + 64 + 32);
    pcm_length = read_u32(bank + 64 + 36);
    second_pcm_offset = read_u32(bank + 64 + 64 + 32);
    second_pcm_length = read_u32(bank + 64 + 64 + 36);
    impulse[0] = 124;
    memset(step, 100, sizeof(step));

    render_file("build/host-tests/default-grains.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_DEFAULT, 7);
    render_file("build/host-tests/freeze-and-dry.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_FREEZE, 8);
    render_file("build/host-tests/reverb-left.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_LEFT, 4);
    render_file("build/host-tests/reverb-right.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_RIGHT, 4);
    render_file("build/host-tests/maximum-load.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_MAXIMUM, 10);
    render_file("build/host-tests/maximum-load-vocal.wav",
                (const int8_t *)(bank + second_pcm_offset), second_pcm_length,
                RENDER_MAXIMUM, 10);
    render_file("build/host-tests/impulse-reverb.wav",
                impulse, sizeof(impulse), RENDER_IMPULSE_REVERB, 3);
    render_file("build/host-tests/freeze-after-impulse.wav",
                impulse, sizeof(impulse), RENDER_IMPULSE_FREEZE, 3);
    render_file("build/host-tests/repeated-freeze.wav",
                impulse, sizeof(impulse), RENDER_REPEATED_FREEZE, 4);
    render_file("build/host-tests/highpass-step.wav",
                step, sizeof(step), RENDER_HIGHPASS_STEP, 2);
    render_file("build/host-tests/lowpass-impulse.wav",
                impulse, sizeof(impulse), RENDER_LOWPASS_IMPULSE, 2);
    render_file("build/host-tests/single-grain.wav",
                (const int8_t *)(bank + pcm_offset), pcm_length,
                RENDER_SINGLE_GRAIN, 2);
    render_file("build/host-tests/maximum-feedback.wav",
                impulse, sizeof(impulse), RENDER_MAXIMUM_FEEDBACK, 10);
    render_file("build/host-tests/maximum-gain.wav",
                step, sizeof(step), RENDER_MAXIMUM_GAIN, 2);
    free(bank);
    puts("offline WAV renders passed: single/default grains, stereo/impulse "
         "reverb, Freeze, filters, maximum feedback/gain and maximum load");
    return 0;
}
