#ifndef AMBGRANULAR_KIOSK_H
#define AMBGRANULAR_KIOSK_H

#include <stdint.h>

enum {
    KIOSK_FRAMES_PER_SECOND = 60,
    KIOSK_MIN_INTERVAL_FRAMES = 30 * KIOSK_FRAMES_PER_SECOND,
    KIOSK_MAX_INTERVAL_FRAMES = 60 * KIOSK_FRAMES_PER_SECOND
};

typedef struct {
    uint32_t random_state;
    uint16_t frames_until_texture;
    uint8_t active;
} KioskState;

void kiosk_init(KioskState *state, uint32_t seed);
int kiosk_cancel_on_input(KioskState *state, uint16_t recognised_input);
int kiosk_tick(KioskState *state);
uint32_t kiosk_choose_sample(KioskState *state, uint32_t sample_count,
                             uint32_t current_sample);
int kiosk_choose_pitch(KioskState *state);

#endif
