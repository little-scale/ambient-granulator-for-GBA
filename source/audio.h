#ifndef AMBGRANULAR_AUDIO_H
#define AMBGRANULAR_AUDIO_H

#include "parameters.h"

#include <gba_types.h>
#include <stdint.h>

enum {
    AUDIO_SAMPLE_RATE = 16384,
    AUDIO_BLOCK_SAMPLES = 512,
    AUDIO_FIFO_READ_AHEAD_SAMPLES = 16,
    AUDIO_FIFO_GUARD_SAMPLES = AUDIO_BLOCK_SAMPLES,
    AUDIO_BLOCK_CYCLES = AUDIO_BLOCK_SAMPLES * 1024
};

extern volatile u32 audio_underruns;
extern volatile u32 audio_blocks_rendered;
extern volatile u32 worst_mix_cycles;

void audio_init(const int8_t *sample, u32 length,
                const ParameterState *parameters, u32 random_seed,
                int startup_center, int startup_grains);
void audio_service(void);
void audio_set_parameters(const ParameterState *parameters);
void audio_set_sample(const int8_t *sample, u32 length);
void audio_trigger_burst(int center_x);
void audio_set_gate(int active, int center_x);
int audio_pop_marker(u8 *x);
u32 audio_grains_started(void);

#endif
