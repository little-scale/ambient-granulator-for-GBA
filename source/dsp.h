#ifndef AMBGRANULAR_DSP_H
#define AMBGRANULAR_DSP_H

#include "parameters.h"

#include <stdint.h>

enum {
    DSP_SAMPLE_RATE = 16384,
    DSP_GRAIN_VOICES = 4,
    DSP_MAX_GRAIN_SAMPLES = 8192,
    DSP_FDN_LINES = 4,
    DSP_FDN_MAX_DELAY = 2048,
    DSP_MARKER_QUEUE_SIZE = 32,
    DSP_FILTER_CHUNK_SAMPLES = 64
};

typedef struct {
    int16_t delay[DSP_FDN_LINES][DSP_FDN_MAX_DELAY];
} DspReverbMemory;

typedef struct {
    const int8_t *source;
    uint32_t position_q16;
    uint32_t step_q16;
    int length;
    int left_gain_q8_12;
    int right_gain_q8_12;
    int left_gain_step_q8_12;
    int right_gain_step_q8_12;
    int remaining;
} DspGrainTail;

typedef struct {
    uint8_t active;
    const int8_t *source;
    uint32_t position_q16;
    uint32_t step_q16;
    int length;
    int attack_samples;
    int release_start;
    int left_gain_q8;
    int right_gain_q8;
    int left_envelope_gain_q8_12;
    int right_envelope_gain_q8_12;
    int left_attack_step_q8_12;
    int right_attack_step_q8_12;
    int left_release_step_q8_12;
    int right_release_step_q8_12;
    DspGrainTail tail;
} DspGrainVoice;

typedef struct {
    int highpass_previous_input;
    int highpass_output;
    int lowpass_output;
} DspFilterState;

typedef struct {
    DspGrainVoice voices[DSP_GRAIN_VOICES];
    DspReverbMemory *reverb_memory;
    int16_t *reverb_delay[DSP_FDN_LINES];
    int reverb_position[DSP_FDN_LINES];
    int reverb_damped[DSP_FDN_LINES];
    int reverb_length[DSP_FDN_LINES];
    int reverb_feedback_q15;
    int reverb_damp_q15;
    int reverb_wet;
    int reverb_wet_q8;
    int reverb_dry_q8;
    uint8_t reverb_freeze;
    uint8_t reverb_active;
    DspFilterState filter[2];
    int highpass_enabled;
    int highpass_alpha_q15;
    int lowpass_enabled;
    int lowpass_alpha_q15;
    ParameterState parameters;
    const int8_t *sample;
    uint32_t sample_length;
    uint32_t random_state;
    uint32_t grains_started;
    int next_voice;
    int grain_length_samples;
    int grain_attack_samples;
    int grain_release_start;
    uint32_t grain_attack_recip_q20;
    uint32_t grain_release_recip_q20;
    int grain_interval_samples;
    int grain_jitter_samples;
    int burst_center;
    int burst_remaining;
    int samples_until_grain;
    uint8_t gate_active;
    int gate_center;
    uint8_t marker_queue[DSP_MARKER_QUEUE_SIZE];
    uint8_t marker_read;
    uint8_t marker_write;
    int32_t grain_left_scratch[DSP_FILTER_CHUNK_SAMPLES];
    int32_t grain_right_scratch[DSP_FILTER_CHUNK_SAMPLES];
    int16_t filter_left_scratch[DSP_FILTER_CHUNK_SAMPLES];
    int16_t filter_right_scratch[DSP_FILTER_CHUNK_SAMPLES];
} DspState;

void dsp_init(DspState *state, DspReverbMemory *reverb_memory);
void dsp_set_parameters(DspState *state, const ParameterState *parameters);
void dsp_set_sample(DspState *state, const int8_t *sample, uint32_t length);
void dsp_set_reverb_line(DspState *state, int line, int16_t *delay);
void dsp_trigger_burst(DspState *state, int center_x);
void dsp_set_gate(DspState *state, int active, int center_x);
void dsp_render(DspState *restrict state, int8_t *restrict left,
                int8_t *restrict right, int count);
int dsp_pop_marker(DspState *state, uint8_t *x);
uint32_t dsp_pitch_step_q16(int semitones);
uint32_t dsp_pitch_step_cents_q16(int total_cents);

#endif
