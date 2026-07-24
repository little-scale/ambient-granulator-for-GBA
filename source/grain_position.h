#ifndef AMBGRANULAR_GRAIN_POSITION_H
#define AMBGRANULAR_GRAIN_POSITION_H

static inline int grain_range_offset(int range, unsigned int random_value)
{
    if (range >= 0)
        return (int)(random_value % (unsigned int)(range * 2 + 1)) - range;

    {
        int magnitude = -range;
        return (int)(random_value % (unsigned int)(magnitude + 1));
    }
}

#endif

