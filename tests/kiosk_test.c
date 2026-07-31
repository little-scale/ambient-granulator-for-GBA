#include "kiosk.h"

#include <assert.h>
#include <stdio.h>

static int valid_pitch(int pitch)
{
    return pitch == -12 || pitch == -7 || pitch == 0
        || pitch == 7 || pitch == 12;
}

int main(void)
{
    KioskState state;
    int seen_pitch[5] = { 0, 0, 0, 0, 0 };
    int index;

    kiosk_init(&state, 0x12345678u);
    assert(state.active);
    assert(state.frames_until_texture >= KIOSK_MIN_INTERVAL_FRAMES);
    assert(state.frames_until_texture <= KIOSK_MAX_INTERVAL_FRAMES);

    state.frames_until_texture = 2;
    assert(!kiosk_tick(&state));
    assert(state.frames_until_texture == 1);
    assert(kiosk_tick(&state));
    assert(state.frames_until_texture >= KIOSK_MIN_INTERVAL_FRAMES);
    assert(state.frames_until_texture <= KIOSK_MAX_INTERVAL_FRAMES);

    for (index = 0; index < 10000; ++index) {
        uint32_t current = (uint32_t)(index % 5);
        uint32_t sample = kiosk_choose_sample(&state, 5, current);
        int pitch = kiosk_choose_pitch(&state);
        assert(sample < 5);
        assert(sample != current);
        assert(valid_pitch(pitch));
        if (pitch == -12)
            seen_pitch[0] = 1;
        else if (pitch == -7)
            seen_pitch[1] = 1;
        else if (pitch == 0)
            seen_pitch[2] = 1;
        else if (pitch == 7)
            seen_pitch[3] = 1;
        else
            seen_pitch[4] = 1;
    }
    for (index = 0; index < 5; ++index)
        assert(seen_pitch[index]);
    assert(kiosk_choose_sample(&state, 0, 0) == 0);
    assert(kiosk_choose_sample(&state, 1, 0) == 0);

    assert(!kiosk_cancel_on_input(&state, 0));
    assert(state.active);
    assert(kiosk_cancel_on_input(&state, 1));
    assert(!state.active);
    assert(state.frames_until_texture == 0);
    assert(!kiosk_tick(&state));
    assert(!kiosk_cancel_on_input(&state, 2));

    puts("kiosk interval, sample, pitch and permanent input cancellation passed");
    return 0;
}
