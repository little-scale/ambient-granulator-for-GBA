#ifndef AMBGRANULAR_GRAIN_TIMING_H
#define AMBGRANULAR_GRAIN_TIMING_H

static inline int grain_interval_samples(int sample_rate, int synced,
                                         int bpm, int denominator,
                                         int interval_ms)
{
    long long numerator;
    int divisor;

    if (synced) {
        numerator = (long long)sample_rate * 240;
        divisor = bpm * denominator;
    } else {
        numerator = (long long)sample_rate * interval_ms;
        divisor = 1000;
    }

    {
        int samples = (int)((numerator + divisor / 2) / divisor);
        return samples < 1 ? 1 : samples;
    }
}

#endif

