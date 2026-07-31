#include "startup.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t seen = 0;
    uint32_t seed;

    assert(startup_mix_seed(0) != 0);
    assert(startup_mix_seed(1) != startup_mix_seed(2));
    assert(startup_sample_index(1234, 0) == 0);
    for (seed = 0; seed < 1000; ++seed) {
        uint32_t index = startup_sample_index(
            startup_mix_seed(seed), 5);
        assert(index < 5);
        seen |= 1u << index;
    }
    assert(seen == 0x1fu);
    assert(startup_freeze_delay_frames(0)
           == STARTUP_REVERB_SETTLE_FRAMES);
    assert(startup_freeze_delay_frames(420) == 34);
    puts("startup sample selection, seed mixing and Freeze delay passed");
    return 0;
}
