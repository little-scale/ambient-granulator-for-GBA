#include "grain_position.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int value;
    int saw_negative = 0;
    int saw_positive = 0;

    assert(grain_range_offset(0, 1234) == 0);
    assert(grain_range_offset(24, 0) == -24);
    assert(grain_range_offset(24, 48) == 24);
    assert(grain_range_offset(-24, 0) == 0);
    assert(grain_range_offset(-24, 24) == 24);
    for (value = 0; value < 10000; ++value) {
        int bipolar = grain_range_offset(128, value);
        int forward = grain_range_offset(-128, value);
        assert(bipolar >= -128 && bipolar <= 128);
        assert(forward >= 0 && forward <= 128);
        saw_negative |= bipolar < 0;
        saw_positive |= bipolar > 0;
    }
    assert(saw_negative && saw_positive);
    puts("position range modes passed");
    return 0;
}
