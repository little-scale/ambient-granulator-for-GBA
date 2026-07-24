#ifndef AMBGRANULAR_AUDIO_CONTROLS_H
#define AMBGRANULAR_AUDIO_CONTROLS_H

static inline int audio_pan_left_percent(int pan)
{
    return pan <= 0 ? 100 : 100 - pan;
}

static inline int audio_pan_right_percent(int pan)
{
    return pan >= 0 ? 100 : 100 + pan;
}

static inline int audio_wet_dry_mix(int dry, int wet, int wet_percent)
{
    return (dry * (100 - wet_percent) + wet * wet_percent) / 100;
}

static inline int audio_db_gain_q12(int decibels)
{
    static const int gains[] = {
        258, 290, 325, 365, 410, 460, 516, 579, 649, 728, 817,
        917, 1029, 1154, 1295, 1453, 1631, 1830, 2053, 2303, 2584,
        2900, 3254, 3651, 4096, 4596, 5157, 5786, 6492, 7284, 8173,
        9170, 10289, 11544, 12953, 14533, 16306, 18296, 20529,
        23034, 25844, 28997, 32536
    };
    if (decibels < -24)
        decibels = -24;
    if (decibels > 18)
        decibels = 18;
    return gains[decibels + 24];
}

static inline void audio_fdn_encode_stereo(int left, int right, int input[4])
{
    input[0] = (left + right) >> 2;
    input[1] = (left - right) >> 2;
    input[2] = (left + right) >> 2;
    input[3] = (-left + right) >> 2;
}

static inline int audio_fdn_decode_left(const int delayed[4])
{
    return (delayed[0] + delayed[1] + delayed[2] - delayed[3]) >> 2;
}

static inline int audio_fdn_decode_right(const int delayed[4])
{
    return (delayed[0] - delayed[1] + delayed[2] + delayed[3]) >> 2;
}

#endif

