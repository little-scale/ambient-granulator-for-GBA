#include "audio_controls.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    int encoded[4];

    assert(audio_pan_left_percent(-100) == 100);
    assert(audio_pan_right_percent(-100) == 0);
    assert(audio_pan_left_percent(0) == 100);
    assert(audio_pan_right_percent(0) == 100);
    assert(audio_pan_left_percent(100) == 0);
    assert(audio_pan_right_percent(100) == 100);
    assert(audio_wet_dry_mix(12000, -4000, 0) == 12000);
    assert(audio_wet_dry_mix(12000, -4000, 100) == -4000);
    assert(audio_db_gain_q12(0) == 4096);
    assert(audio_db_gain_q12(-100) == audio_db_gain_q12(-24));
    assert(audio_db_gain_q12(100) == audio_db_gain_q12(18));

    audio_fdn_encode_stereo(12000, -4000, encoded);
    assert(audio_fdn_decode_left(encoded) == 3000);
    assert(audio_fdn_decode_right(encoded) == -1000);
    audio_fdn_encode_stereo(-4000, 12000, encoded);
    assert(audio_fdn_decode_left(encoded) == -1000);
    assert(audio_fdn_decode_right(encoded) == 3000);
    puts("pan, gain, wet/dry and stereo FDN controls passed");
    return 0;
}
