#include "audio_handoff.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int elapsed;

    assert(audio_handoff_resume_offset(0) == 0);
    assert(audio_handoff_resume_offset(1) == 16);
    assert(audio_handoff_resume_offset(15) == 16);
    assert(audio_handoff_resume_offset(16) == 16);
    assert(audio_handoff_resume_offset(17) == 32);
    for (elapsed = 0; elapsed <= 512; ++elapsed)
        assert(audio_handoff_resume_offset(elapsed)
               == ((elapsed + 15u) / 16u) * 16u);
    puts("FIFO handoff lateness rounding and on-time sample-zero resume passed");
    return 0;
}
