#ifndef AMBGRANULAR_AUDIO_HANDOFF_H
#define AMBGRANULAR_AUDIO_HANDOFF_H

#include <stdint.h>

enum { AUDIO_HANDOFF_DMA_SAMPLES = 16 };

/*
 * A FIFO DMA request moves 16 samples.  The block timer fires one request
 * early, so an on-time IRQ has consumed none of the mirrored next block.
 * Only lateness after that tick can have queued mirrored samples.
 */
static inline uint32_t audio_handoff_resume_offset(uint32_t elapsed_samples)
{
    return (elapsed_samples + AUDIO_HANDOFF_DMA_SAMPLES - 1u)
        & ~(AUDIO_HANDOFF_DMA_SAMPLES - 1u);
}

#endif
