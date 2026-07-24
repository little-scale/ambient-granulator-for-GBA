#include "dsp.h"
#include "grain_timing.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DspState state_a;
static DspState state_b;
static DspReverbMemory reverb_a;
static DspReverbMemory reverb_b;
static int8_t sample_data[32768];
static int8_t left_a[256];
static int8_t right_a[256];
static int8_t left_b[256];
static int8_t right_b[256];

static ParameterState dry_parameters(void)
{
    ParameterState parameters;
    parameters_reset(&parameters);
    parameters.value[PARAM_REVERB] = 0;
    parameters.value[PARAM_REVERB_FEEDBACK] = 0;
    parameters.value[PARAM_RANGE] = 0;
    parameters.value[PARAM_PITCH] = 0;
    parameters.value[PARAM_FINE] = 0;
    parameters.value[PARAM_PITCH_DEVIATION] = 0;
    parameters.value[PARAM_FINE_DEVIATION] = 0;
    parameters.value[PARAM_PAN_DEVIATION] = 0;
    parameters.value[PARAM_CLOCK] = 0;
    parameters.value[PARAM_INTERVAL] = 20;
    parameters.value[PARAM_JITTER] = 0;
    parameters.value[PARAM_GRAINS] = 1;
    parameters.value[PARAM_LENGTH] = 20;
    parameters.value[PARAM_ATTACK] = 0;
    parameters.value[PARAM_RELEASE] = 0;
    parameters.value[PARAM_GAIN] = 0;
    parameters.value[PARAM_VOLUME] = 100;
    parameters.value[PARAM_HIGHPASS] = 0;
    parameters.value[PARAM_LOWPASS] = 8000;
    return parameters;
}

static void reset_with(const ParameterState *parameters)
{
    dsp_init(&state_a, &reverb_a);
    dsp_set_parameters(&state_a, parameters);
    dsp_set_sample(&state_a, sample_data, sizeof(sample_data));
}

static void test_pitch_pan_and_envelope(void)
{
    ParameterState parameters = dry_parameters();
    int index;

    assert(dsp_pitch_step_q16(-24) == 16384);
    assert(dsp_pitch_step_q16(0) == 65536);
    assert(dsp_pitch_step_q16(24) == 262144);
    assert(dsp_pitch_step_cents_q16(-2500) == 15464);
    assert(dsp_pitch_step_cents_q16(-50) == 63696);
    assert(dsp_pitch_step_cents_q16(50) == 67484);
    assert(dsp_pitch_step_cents_q16(2500) == 277732);
    parameters.value[PARAM_FINE] = 50;
    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 1);
    assert(state_a.voices[0].step_q16 == 67484);
    parameters.value[PARAM_FINE] = 0;
    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 64);
    for (index = 0; index < 64; ++index) {
        assert(left_a[index] != 0);
        assert(right_a[index] == 0);
    }

    parameters.value[PARAM_PAN] = 100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 64);
    for (index = 0; index < 64; ++index) {
        assert(left_a[index] == 0);
        assert(right_a[index] != 0);
    }

    parameters.value[PARAM_PAN] = -100;
    parameters.value[PARAM_ATTACK] = 50;
    parameters.value[PARAM_RELEASE] = 50;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 256);
    assert(left_a[0] == 0);
    assert(left_a[160] > left_a[32]);
    dsp_render(&state_a, left_a, right_a, 72);
    assert(left_a[70] == 0);
}

static void test_scheduler(void)
{
    ParameterState parameters = dry_parameters();
    int base = grain_interval_samples(DSP_SAMPLE_RATE, 0, 98, 8, 20);
    int first = -1;
    int second = -1;
    int frame;

    parameters.value[PARAM_GRAINS] = 2;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 120);
    for (frame = 0; frame <= base + 2; ++frame) {
        uint32_t before = state_a.grains_started;
        dsp_render(&state_a, left_a, right_a, 1);
        if (state_a.grains_started != before) {
            if (first < 0)
                first = frame;
            else
                second = frame;
        }
    }
    assert(first == 0);
    assert(second == base);

    parameters.value[PARAM_GRAINS] = 3;
    reset_with(&parameters);
    dsp_set_gate(&state_a, 1, 120);
    dsp_render(&state_a, left_a, right_a, 1);
    dsp_set_gate(&state_a, 0, 120);
    for (frame = 0; frame < base * 3; ++frame)
        dsp_render(&state_a, left_a, right_a, 1);
    assert(state_a.grains_started == 3);

    parameters.value[PARAM_GRAINS] = 2;
    parameters.value[PARAM_JITTER] = 100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 120);
    dsp_render(&state_a, left_a, right_a, 1);
    assert(state_a.samples_until_grain >= 1);
    assert(state_a.samples_until_grain <= base * 2);
}

static void test_filters(void)
{
    ParameterState parameters = dry_parameters();

    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 128);
    assert(left_a[0] == left_a[127]);

    parameters.value[PARAM_LOWPASS] = 200;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 128);
    assert(left_a[0] > 0 && left_a[0] < left_a[127]);
    assert(left_a[127] <= 124);

    parameters.value[PARAM_LOWPASS] = 8000;
    parameters.value[PARAM_HIGHPASS] = 500;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 128);
    assert(left_a[0] > 0);
    assert(left_a[127] >= -1 && left_a[127] <= 1);
}

static void test_maximum_gain_saturation(void)
{
    ParameterState parameters = dry_parameters();

    parameters.value[PARAM_PAN] = -100;
    parameters.value[PARAM_GAIN] = 18;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 1);
    assert(left_a[0] == 127);
    assert(right_a[0] == 0);
}

static void test_freeze(void)
{
    ParameterState parameters = dry_parameters();
    int block;
    int index;
    int saw_texture = 0;

    parameters.value[PARAM_REVERB] = 100;
    parameters.value[PARAM_REVERB_FEEDBACK] = 990;
    parameters.value[PARAM_REVERB_DAMP] = 10;
    parameters.value[PARAM_REVERB_SIZE] = 100;
    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    for (block = 0; block < 8; ++block)
        dsp_render(&state_a, left_a, right_a, 256);

    parameters.value[PARAM_REVERB_FREEZE] = 1;
    dsp_set_parameters(&state_a, &parameters);
    state_b = state_a;
    memcpy(&reverb_b, &reverb_a, sizeof(reverb_b));
    state_b.reverb_memory = &reverb_b;
    for (index = 0; index < DSP_FDN_LINES; ++index)
        state_b.reverb_delay[index] = reverb_b.delay[index];
    dsp_trigger_burst(&state_a, 120);
    for (block = 0; block < 16; ++block) {
        dsp_render(&state_a, left_a, right_a, 256);
        dsp_render(&state_b, left_b, right_b, 256);
        assert(memcmp(left_a, left_b, sizeof(left_a)) == 0);
        assert(memcmp(right_a, right_b, sizeof(right_a)) == 0);
        for (index = 0; index < 256; ++index)
            saw_texture |= left_a[index] != 0 || right_a[index] != 0;
    }
    assert(saw_texture);
    assert(memcmp(&reverb_a, &reverb_b, sizeof(reverb_a)) == 0);

    saw_texture = 0;
    for (block = 0; block < DSP_SAMPLE_RATE * 60 / 256; ++block) {
        dsp_render(&state_a, left_a, right_a, 256);
        for (index = 0; index < 256; ++index) {
            assert(left_a[index] >= -128 && left_a[index] <= 127);
            assert(right_a[index] >= -128 && right_a[index] <= 127);
            saw_texture |= left_a[index] != 0 || right_a[index] != 0;
        }
    }
    assert(saw_texture);
}

static void test_filter_shapes_freeze_without_altering_fdn(void)
{
    ParameterState parameters = dry_parameters();
    int block;
    int index;
    int output_differs = 0;

    parameters.value[PARAM_REVERB] = 100;
    parameters.value[PARAM_REVERB_FEEDBACK] = 990;
    parameters.value[PARAM_REVERB_DAMP] = 10;
    parameters.value[PARAM_REVERB_SIZE] = 100;
    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    dsp_trigger_burst(&state_a, 0);
    for (block = 0; block < 8; ++block)
        dsp_render(&state_a, left_a, right_a, 256);

    parameters.value[PARAM_REVERB_FREEZE] = 1;
    dsp_set_parameters(&state_a, &parameters);
    state_b = state_a;
    memcpy(&reverb_b, &reverb_a, sizeof(reverb_b));
    state_b.reverb_memory = &reverb_b;
    for (index = 0; index < DSP_FDN_LINES; ++index)
        state_b.reverb_delay[index] = reverb_b.delay[index];
    parameters.value[PARAM_LOWPASS] = 200;
    dsp_set_parameters(&state_b, &parameters);

    for (block = 0; block < 16; ++block) {
        dsp_render(&state_a, left_a, right_a, 256);
        dsp_render(&state_b, left_b, right_b, 256);
        for (index = 0; index < 256; ++index)
            output_differs |= left_a[index] != left_b[index]
                           || right_a[index] != right_b[index];
        assert(memcmp(&reverb_a, &reverb_b, sizeof(reverb_a)) == 0);
    }
    assert(output_differs);
}

static int64_t reverb_energy(const DspState *state)
{
    int64_t energy = 0;
    int line;
    for (line = 0; line < DSP_FDN_LINES; ++line) {
        int index;
        for (index = 0; index < state->reverb_length[line]; ++index) {
            int value = state->reverb_delay[line][index];
            energy += (int64_t)value * value;
        }
    }
    return energy;
}

static void test_maximum_feedback(void)
{
    ParameterState parameters = dry_parameters();
    int64_t initial_energy;
    int64_t final_energy;
    int block;

    parameters.value[PARAM_REVERB] = 100;
    parameters.value[PARAM_REVERB_FEEDBACK] = 999;
    parameters.value[PARAM_REVERB_DAMP] = 0;
    parameters.value[PARAM_REVERB_SIZE] = 100;
    parameters.value[PARAM_PAN] = -100;
    reset_with(&parameters);
    assert(state_a.reverb_feedback_q15 == 32735);
    dsp_trigger_burst(&state_a, 0);
    for (block = 0; block < DSP_SAMPLE_RATE / 256; ++block)
        dsp_render(&state_a, left_a, right_a, 256);
    initial_energy = reverb_energy(&state_a);
    assert(initial_energy > 0);
    for (block = 0; block < DSP_SAMPLE_RATE * 60 / 256; ++block)
        dsp_render(&state_a, left_a, right_a, 256);
    final_energy = reverb_energy(&state_a);
    assert(final_energy <= initial_energy);
}

static void test_voice_steal_continues_waveform(void)
{
    ParameterState parameters = dry_parameters();
    DspGrainVoice *voice;
    int index;

    for (index = 0; index < (int)sizeof(sample_data); ++index)
        sample_data[index] = (int8_t)((index & 1) ? -100 : 100);
    parameters.value[PARAM_VOLUME] = 0;
    reset_with(&parameters);

    voice = &state_a.voices[0];
    voice->active = 1;
    voice->source = sample_data;
    voice->position_q16 = 10u << 16;
    voice->step_q16 = 1u << 16;
    voice->length = 1024;
    voice->attack_samples = 0;
    voice->release_start = voice->length;
    voice->left_gain_q8 = 256;
    voice->right_gain_q8 = 0;
    voice->left_envelope_gain_q8_12 = 256 << 12;
    voice->right_envelope_gain_q8_12 = 0;
    state_a.next_voice = 0;

    dsp_trigger_burst(&state_a, 0);
    dsp_render(&state_a, left_a, right_a, 32);
    assert(voice->tail.remaining == 0);
    assert(voice->tail.position_q16 == (42u << 16));
    assert(voice->tail.left_gain_q8_12 == 0);
    assert(voice->tail.right_gain_q8_12 == 0);
    for (index = 0; index < 31; ++index) {
        assert((index & 1) ? left_a[index] < 0 : left_a[index] > 0);
        assert(right_a[index] == 0);
        if (index > 1)
            assert(abs((int)left_a[index]) <= abs((int)left_a[index - 2]));
    }
    assert(left_a[31] == 0 && right_a[31] == 0);
    dsp_render(&state_a, left_a, right_a, 1);
    assert(left_a[0] == 0 && right_a[0] == 0);
}

int main(void)
{
    memset(sample_data, 124, sizeof(sample_data));
    test_pitch_pan_and_envelope();
    test_scheduler();
    test_filters();
    test_maximum_gain_saturation();
    test_maximum_feedback();
    test_freeze();
    test_filter_shapes_freeze_without_altering_fdn();
    test_voice_steal_continues_waveform();
    puts("DSP pitch, envelope, scheduler, gain saturation, 99.9% feedback, "
         "filters including frozen texture, 60-second Freeze and "
         "waveform-continuous voice stealing passed");
    return 0;
}
