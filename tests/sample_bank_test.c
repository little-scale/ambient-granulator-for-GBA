#include "sample_bank.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

int main(void)
{
    FILE *file = fopen("assets/sample_bank.bin", "rb");
    uint8_t *data;
    uint8_t *copy;
    long size;
    uint32_t pcm_offset;
    uint32_t index;
    uint32_t test_index = 0;
    SampleBank bank;
    SampleBankEntry entry;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    data = malloc((size_t)size);
    copy = malloc((size_t)size);
    assert(data != NULL && copy != NULL);
    assert(fread(data, 1, (size_t)size, file) == (size_t)size);
    fclose(file);

    assert(sample_bank_open(&bank, data, (size_t)size));
    assert(bank.count >= 1);
    for (index = 0; index < bank.count; ++index) {
        assert(sample_bank_get(&bank, index, &entry));
        assert(entry.name[0] != '\0');
        assert((((uintptr_t)entry.pcm - (uintptr_t)data) & 31u) == 0);
    }
    assert(sample_bank_get(&bank, test_index, &entry));
    assert(entry.length > 0);
    pcm_offset = read_u32(data + 64 + test_index * 64 + 32);

    memcpy(copy, data, (size_t)size);
    write_u32(copy + 64 + test_index * 64 + 32, pcm_offset + 1);
    assert(!sample_bank_open(&bank, copy, (size_t)size));

    memcpy(copy, data, (size_t)size);
    memset(copy + 64 + test_index * 64, 'X', 32);
    assert(!sample_bank_open(&bank, copy, (size_t)size));

    memcpy(copy, data, (size_t)size);
    write_u32(copy + 20, read_u32(copy + 20) - 1);
    assert(!sample_bank_open(&bank, copy, (size_t)size));

    free(copy);
    free(data);
    puts("runtime sample-bank layout, bounds and alignment validation passed");
    return 0;
}
