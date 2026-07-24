#include "sample_bank.h"

#ifndef SAMPLE_BANK_NO_EMBEDDED
#include "sample_bank_bin.h"
#endif

enum {
    HEADER_SIZE = 64,
    ENTRY_SIZE = 64,
    DATA_OFFSET = HEADER_SIZE + ENTRY_SIZE * SAMPLE_BANK_MAX_ENTRIES,
    WAVEFORM_BYTES = SAMPLE_BANK_WAVEFORM_COLUMNS * 2
};

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0]
         | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16)
         | ((uint32_t)data[3] << 24);
}

static int range_valid(uint32_t offset, uint32_t length, uint32_t limit)
{
    return offset <= limit && length <= limit - offset;
}

static int entry_valid(const uint8_t *bytes, uint32_t index, uint32_t used)
{
    const uint8_t *entry = bytes + HEADER_SIZE + index * ENTRY_SIZE;
    uint32_t pcm_offset = read_u32(entry + 32);
    uint32_t pcm_length = read_u32(entry + 36);
    uint32_t waveform_offset = read_u32(entry + 44);
    uint32_t waveform_length = read_u32(entry + 48);
    unsigned int name_length;

    for (name_length = 0; name_length < 32 && entry[name_length] != 0;
         ++name_length) {
    }
    if (name_length == 32
            || pcm_length == 0
            || pcm_offset < DATA_OFFSET || (pcm_offset & 31u) != 0
            || waveform_offset < DATA_OFFSET || (waveform_offset & 31u) != 0
            || !range_valid(pcm_offset, pcm_length, used)
            || waveform_length != WAVEFORM_BYTES
            || !range_valid(waveform_offset, waveform_length, used))
        return 0;
    return 1;
}

int sample_bank_open(SampleBank *bank, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    const volatile uint8_t *signature = bytes;
    uint32_t capacity;
    uint32_t count;
    uint32_t used;
    uint32_t index;

    if (bank == NULL || bytes == NULL || size < DATA_OFFSET)
        return 0;
    if (signature[0] != 0x47 || signature[1] != 0x42
            || signature[2] != 0x41 || signature[3] != 0x47
            || signature[4] != 0x52 || signature[5] != 0x4e
            || signature[6] != 0x30 || signature[7] != 0x31)
        return 0;
    capacity = read_u32(bytes + 12);
    count = read_u32(bytes + 16);
    used = read_u32(bytes + 20);
    if (read_u32(bytes + 8) != 1
            || capacity > size
            || used < DATA_OFFSET || used > capacity
            || count == 0 || count > SAMPLE_BANK_MAX_ENTRIES
            || read_u32(bytes + 24) != SAMPLE_BANK_RATE
            || read_u32(bytes + 28) != ENTRY_SIZE
            || read_u32(bytes + 32) != SAMPLE_BANK_MAX_ENTRIES
            || read_u32(bytes + 36) != DATA_OFFSET
            || read_u32(bytes + 40) != 1
            || read_u32(bytes + 44) != SAMPLE_BANK_WAVEFORM_COLUMNS)
        return 0;

    for (index = 0; index < count; ++index)
        if (!entry_valid(bytes, index, used))
            return 0;

    bank->base = bytes;
    bank->capacity = capacity;
    bank->used_bytes = used;
    bank->count = count;
    return 1;
}

int sample_bank_open_embedded(SampleBank *bank)
{
#ifdef SAMPLE_BANK_NO_EMBEDDED
    (void)bank;
    return 0;
#else
    return sample_bank_open(bank, sample_bank_bin, sample_bank_bin_size);
#endif
}

int sample_bank_get(const SampleBank *bank, uint32_t index,
                    SampleBankEntry *entry)
{
    const uint8_t *source;
    uint32_t pcm_offset;
    uint32_t pcm_length;
    uint32_t waveform_offset;
    uint32_t waveform_length;
    unsigned int name_length;

    if (bank == NULL || entry == NULL || index >= bank->count)
        return 0;
    source = bank->base + HEADER_SIZE + index * ENTRY_SIZE;
    pcm_offset = read_u32(source + 32);
    pcm_length = read_u32(source + 36);
    waveform_offset = read_u32(source + 44);
    waveform_length = read_u32(source + 48);
    if (pcm_length == 0
            || pcm_offset < DATA_OFFSET || (pcm_offset & 31u) != 0
            || waveform_offset < DATA_OFFSET || (waveform_offset & 31u) != 0
            || !range_valid(pcm_offset, pcm_length, bank->used_bytes)
            || waveform_length != WAVEFORM_BYTES
            || !range_valid(waveform_offset, waveform_length, bank->used_bytes))
        return 0;

    for (name_length = 0; name_length < 32 && source[name_length] != 0;
         ++name_length)
        entry->name[name_length] = (char)source[name_length];
    entry->name[name_length] = '\0';
    entry->pcm = (const int8_t *)(bank->base + pcm_offset);
    entry->length = pcm_length;
    entry->crc32 = read_u32(source + 40);
    entry->minimums = (const int8_t *)(bank->base + waveform_offset);
    entry->maximums = entry->minimums + SAMPLE_BANK_WAVEFORM_COLUMNS;
    return 1;
}
