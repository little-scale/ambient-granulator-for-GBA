#ifndef AMBGRANULAR_SAMPLE_BANK_H
#define AMBGRANULAR_SAMPLE_BANK_H

#include <stddef.h>
#include <stdint.h>

enum {
    SAMPLE_BANK_RATE = 16384,
    SAMPLE_BANK_MAX_ENTRIES = 64,
    SAMPLE_BANK_WAVEFORM_COLUMNS = 240
};

typedef struct {
    const uint8_t *base;
    uint32_t capacity;
    uint32_t used_bytes;
    uint32_t count;
} SampleBank;

typedef struct {
    char name[33];
    const int8_t *pcm;
    uint32_t length;
    uint32_t crc32;
    const int8_t *minimums;
    const int8_t *maximums;
} SampleBankEntry;

int sample_bank_open(SampleBank *bank, const void *data, size_t size);
int sample_bank_open_embedded(SampleBank *bank);
int sample_bank_get(const SampleBank *bank, uint32_t index,
                    SampleBankEntry *entry);

#endif

