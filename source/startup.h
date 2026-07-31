#ifndef AMBGRANULAR_STARTUP_H
#define AMBGRANULAR_STARTUP_H

#include <stdint.h>

enum {
    STARTUP_GRAIN_COUNT = 8,
    STARTUP_REVERB_SETTLE_FRAMES = 8
};

uint32_t startup_mix_seed(uint32_t value);
uint32_t startup_next_seed(uint32_t entropy);
uint32_t startup_sample_index(uint32_t seed, uint32_t sample_count);
int startup_freeze_delay_frames(int grain_length_ms);

#endif
