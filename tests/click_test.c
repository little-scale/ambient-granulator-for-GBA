#include "dsp.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static DspState state;
static DspReverbMemory reverb;
static int8_t left[512];
static int8_t right[512];
static int previous_left;
static int previous_right;
static int maximum_jump_left;
static int maximum_jump_right;

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void render_and_measure(int frames)
{
    while (frames > 0) {
        int count = frames > 512 ? 512 : frames;
        int index;
        dsp_render(&state, left, right, count);
        for (index = 0; index < count; ++index) {
            int jump_left = abs((int)left[index] - previous_left);
            int jump_right = abs((int)right[index] - previous_right);
            if (jump_left > maximum_jump_left)
                maximum_jump_left = jump_left;
            if (jump_right > maximum_jump_right)
                maximum_jump_right = jump_right;
            previous_left = left[index];
            previous_right = right[index];
        }
        frames -= count;
    }
}

static void reset_jump_measurement(void)
{
    maximum_jump_left = 0;
    maximum_jump_right = 0;
}

static void edit_and_measure(ParameterState *parameters, ParameterId id,
                             int value, const char *label)
{
    reset_jump_measurement();
    parameters->value[id] = value;
    dsp_set_parameters(&state, parameters);
    render_and_measure(512);
    printf("parameter transition %-9s L=%d R=%d\n", label,
           maximum_jump_left, maximum_jump_right);
    assert(maximum_jump_left <= 24 && maximum_jump_right <= 24);
}

int main(void)
{
    FILE *file = fopen("assets/sample_bank.bin", "rb");
    uint8_t *bank;
    long size;
    uint32_t pcm_offset;
    uint32_t pcm_length;
    ParameterState parameters;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    bank = malloc((size_t)size);
    assert(bank != NULL && fread(bank, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    pcm_offset = read_u32(bank + 64 + 32);
    pcm_length = read_u32(bank + 64 + 36);

    parameters_reset(&parameters);
    parameters.value[PARAM_LENGTH] += 10;
    dsp_init(&state, &reverb);
    dsp_set_parameters(&state, &parameters);
    dsp_set_sample(&state, (const int8_t *)(bank + pcm_offset), pcm_length);
    dsp_set_gate(&state, 1, 121);
    render_and_measure(DSP_SAMPLE_RATE);
    dsp_set_gate(&state, 0, 121);
    render_and_measure(DSP_SAMPLE_RATE / 2);
    parameters.value[PARAM_REVERB_FREEZE] = 1;
    dsp_set_parameters(&state, &parameters);
    render_and_measure(DSP_SAMPLE_RATE / 2);
    dsp_set_sample(&state, (const int8_t *)(bank + pcm_offset), pcm_length);
    dsp_set_gate(&state, 1, 121);
    render_and_measure(DSP_SAMPLE_RATE * 2);
    dsp_set_gate(&state, 0, 121);
    render_and_measure(DSP_SAMPLE_RATE / 2);
    printf("interactive PCM8 maximum adjacent jump: L=%d R=%d\n",
           maximum_jump_left, maximum_jump_right);
    assert(maximum_jump_left <= 24 && maximum_jump_right <= 24);

    parameters_reset(&parameters);
    dsp_init(&state, &reverb);
    dsp_set_parameters(&state, &parameters);
    dsp_set_sample(&state, (const int8_t *)(bank + pcm_offset), pcm_length);
    dsp_set_gate(&state, 1, 121);
    render_and_measure(DSP_SAMPLE_RATE * 2);
    edit_and_measure(&parameters, PARAM_REVERB, 0, "REV 0");
    edit_and_measure(&parameters, PARAM_REVERB, 100, "REV 100");
    edit_and_measure(&parameters, PARAM_REVERB_SIZE, 0, "SIZE 0");
    edit_and_measure(&parameters, PARAM_REVERB_SIZE, 100, "SIZE 100");
    edit_and_measure(&parameters, PARAM_REVERB_DAMP, 100, "DAMP 100");
    edit_and_measure(&parameters, PARAM_REVERB_DAMP, 0, "DAMP 0");
    edit_and_measure(&parameters, PARAM_REVERB_FEEDBACK, 0, "FDBK 0");
    edit_and_measure(&parameters, PARAM_REVERB_FEEDBACK, 999, "FDBK 99.9");
    edit_and_measure(&parameters, PARAM_HIGHPASS, 4000, "HPF 4000");
    edit_and_measure(&parameters, PARAM_HIGHPASS, 0, "HPF OFF");
    edit_and_measure(&parameters, PARAM_LOWPASS, 200, "LPF 200");
    edit_and_measure(&parameters, PARAM_LOWPASS, 8000, "LPF OFF");
    edit_and_measure(&parameters, PARAM_REVERB_FREEZE, 1, "FREEZE ON");
    edit_and_measure(&parameters, PARAM_REVERB_FREEZE, 0, "FREEZE OFF");

    parameters_reset(&parameters);
    parameters.value[PARAM_CLOCK] = 0;
    parameters.value[PARAM_INTERVAL] = 20;
    parameters.value[PARAM_JITTER] = 0;
    parameters.value[PARAM_GRAINS] = 32;
    parameters.value[PARAM_LENGTH] = 500;
    parameters.value[PARAM_ATTACK] = 10;
    parameters.value[PARAM_RELEASE] = 10;
    parameters.value[PARAM_REVERB] = 0;
    parameters.value[PARAM_LOWPASS] = 8000;
    dsp_init(&state, &reverb);
    dsp_set_parameters(&state, &parameters);
    dsp_set_sample(&state, (const int8_t *)(bank + pcm_offset), pcm_length);
    dsp_set_gate(&state, 1, 121);
    reset_jump_measurement();
    render_and_measure(DSP_SAMPLE_RATE * 4);
    printf("maximum-density voice-steal jump: L=%d R=%d\n",
           maximum_jump_left, maximum_jump_right);
    assert(maximum_jump_left <= 24 && maximum_jump_right <= 24);
    free(bank);
    return 0;
}
