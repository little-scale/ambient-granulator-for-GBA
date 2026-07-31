#include "startup.h"

#if defined(__arm__)
enum {
    STARTUP_SRAM_BASE = 0x0e000000,
    STARTUP_SRAM_MAGIC = 0x314e5247,
    STARTUP_SRAM_CHECK_XOR = 0xa5c36f29
};

/*
 * mGBA and flashcart tooling use this conventional signature to identify the
 * save type. The volatile read in startup_next_seed keeps it in the ROM.
 */
static const volatile char gba_save_type[] = "SRAM_V113";

static uint32_t sram_read_u32(unsigned int offset)
{
    volatile const uint8_t *sram
        = (volatile const uint8_t *)STARTUP_SRAM_BASE;
    return (uint32_t)sram[offset]
        | ((uint32_t)sram[offset + 1] << 8)
        | ((uint32_t)sram[offset + 2] << 16)
        | ((uint32_t)sram[offset + 3] << 24);
}

static void sram_write_u32(unsigned int offset, uint32_t value)
{
    volatile uint8_t *sram = (volatile uint8_t *)STARTUP_SRAM_BASE;
    sram[offset] = (uint8_t)value;
    sram[offset + 1] = (uint8_t)(value >> 8);
    sram[offset + 2] = (uint8_t)(value >> 16);
    sram[offset + 3] = (uint8_t)(value >> 24);
}
#endif

uint32_t startup_mix_seed(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value != 0 ? value : 0x6d2b79f5u;
}

uint32_t startup_next_seed(uint32_t entropy)
{
    uint32_t state;

#if defined(__arm__)
    uint32_t magic = sram_read_u32(0);
    uint32_t saved = sram_read_u32(4);
    uint32_t check = sram_read_u32(8);
    entropy ^= (uint32_t)(uint8_t)gba_save_type[0] << 24;
    if (magic == STARTUP_SRAM_MAGIC
            && check == (saved ^ STARTUP_SRAM_CHECK_XOR)) {
        state = saved;
    } else {
        state = entropy ^ 0x9e3779b9u;
    }
    state = startup_mix_seed(state ^ entropy);
    sram_write_u32(0, STARTUP_SRAM_MAGIC);
    sram_write_u32(4, state);
    sram_write_u32(8, state ^ STARTUP_SRAM_CHECK_XOR);
#else
    state = startup_mix_seed(entropy ^ 0x9e3779b9u);
#endif

    return state;
}

uint32_t startup_sample_index(uint32_t seed, uint32_t sample_count)
{
    if (sample_count == 0)
        return 0;
    return (uint32_t)(((uint64_t)seed * sample_count) >> 32);
}

int startup_freeze_delay_frames(int grain_length_ms)
{
    int grain_frames;
    if (grain_length_ms < 0)
        grain_length_ms = 0;
    grain_frames = (grain_length_ms * 60 + 999) / 1000;
    return grain_frames + STARTUP_REVERB_SETTLE_FRAMES;
}
