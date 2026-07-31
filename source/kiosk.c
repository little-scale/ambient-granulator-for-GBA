#include "kiosk.h"

static uint32_t next_random(KioskState *state)
{
    uint32_t value = state->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0)
        value = 0x6d2b79f5u;
    state->random_state = value;
    return value;
}

static uint16_t choose_interval(KioskState *state)
{
    uint32_t range = KIOSK_MAX_INTERVAL_FRAMES
                   - KIOSK_MIN_INTERVAL_FRAMES + 1u;
    uint32_t offset = (uint32_t)(((uint64_t)next_random(state) * range) >> 32);
    return (uint16_t)(KIOSK_MIN_INTERVAL_FRAMES + offset);
}

void kiosk_init(KioskState *state, uint32_t seed)
{
    state->random_state = seed ^ 0x4b494f53u;
    if (state->random_state == 0)
        state->random_state = 0x6d2b79f5u;
    state->active = 1;
    state->frames_until_texture = choose_interval(state);
}

int kiosk_cancel_on_input(KioskState *state, uint16_t recognised_input)
{
    if (!state->active || recognised_input == 0)
        return 0;
    state->active = 0;
    state->frames_until_texture = 0;
    return 1;
}

int kiosk_tick(KioskState *state)
{
    if (!state->active)
        return 0;
    if (state->frames_until_texture > 1) {
        --state->frames_until_texture;
        return 0;
    }
    state->frames_until_texture = choose_interval(state);
    return 1;
}

uint32_t kiosk_choose_sample(KioskState *state, uint32_t sample_count,
                             uint32_t current_sample)
{
    uint32_t choice;
    if (sample_count <= 1)
        return 0;
    if (current_sample >= sample_count)
        current_sample = 0;
    choice = (uint32_t)(((uint64_t)next_random(state)
                        * (sample_count - 1u)) >> 32);
    return choice >= current_sample ? choice + 1u : choice;
}

int kiosk_choose_pitch(KioskState *state)
{
    static const int pitches[5] = { -12, -7, 0, 7, 12 };
    uint32_t index = (uint32_t)(((uint64_t)next_random(state) * 5u) >> 32);
    return pitches[index];
}
