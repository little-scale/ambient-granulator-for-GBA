#include "grain_timing.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    static const int divisions[] = { 8, 16, 32, 64 };
    int bpm;
    int division;
    int interval;

    for (bpm = 40; bpm <= 240; ++bpm) {
        for (division = 0; division < 4; ++division) {
            long long numerator = (long long)16384 * 240;
            int divisor = bpm * divisions[division];
            int expected = (int)((numerator + divisor / 2) / divisor);
            assert(grain_interval_samples(16384, 1, bpm,
                                          divisions[division], 120)
                   == expected);
        }
    }
    for (interval = 20; interval <= 1000; interval += 10) {
        int expected = (16384 * interval + 500) / 1000;
        assert(grain_interval_samples(16384, 0, 98, 8, interval)
               == expected);
    }
    assert(grain_interval_samples(1, 0, 98, 8, 1) == 1);
    puts("timing conversions passed");
    return 0;
}
