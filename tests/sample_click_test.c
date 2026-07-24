#include "dsp.h"
#include "sample_bank.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { BLOCK = 512 };

static DspState state;
static DspReverbMemory reverb;
static int8_t left[BLOCK];
static int8_t right[BLOCK];

static int maximum_source_jump(const int8_t *sample, uint32_t length)
{
    int maximum = 0;
    uint32_t index;
    for (index = 1; index < length; ++index) {
        int jump = abs((int)sample[index] - (int)sample[index - 1]);
        if (jump > maximum)
            maximum = jump;
    }
    return maximum;
}

static int render_maximum_jump(const SampleBankEntry *entry,
                               ParameterState *parameters, int seconds)
{
    static const int positions[5] = { 0, 60, 120, 180, 239 };
    int maximum = 0;
    int previous_left = 0;
    int previous_right = 0;
    int blocks = seconds * DSP_SAMPLE_RATE / BLOCK;
    int block;

    dsp_init(&state, &reverb);
    dsp_set_parameters(&state, parameters);
    dsp_set_sample(&state, entry->pcm, entry->length);
    dsp_set_gate(&state, 1, positions[0]);
    for (block = 0; block < blocks; ++block) {
        int index;
        dsp_set_gate(&state, 1, positions[(block / 16) % 5]);
        dsp_render(&state, left, right, BLOCK);
        for (index = 0; index < BLOCK; ++index) {
            int jump_left = abs((int)left[index] - previous_left);
            int jump_right = abs((int)right[index] - previous_right);
            if (jump_left > maximum)
                maximum = jump_left;
            if (jump_right > maximum)
                maximum = jump_right;
            previous_left = left[index];
            previous_right = right[index];
        }
    }
    return maximum;
}

int main(void)
{
    FILE *file = fopen("assets/sample_bank.bin", "rb");
    SampleBank bank;
    uint8_t *bytes;
    long size;
    uint32_t sample_index;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)size);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    assert(fclose(file) == 0);
    assert(sample_bank_open(&bank, bytes, (size_t)size));

    for (sample_index = 0; sample_index < bank.count; ++sample_index) {
        SampleBankEntry entry;
        ParameterState parameters;
        int normal_jump;
        int dry_jump;
        int filtered_jump;

        assert(sample_bank_get(&bank, sample_index, &entry));
        parameters_reset(&parameters);
        normal_jump = render_maximum_jump(&entry, &parameters, 4);

        parameters.value[PARAM_CLOCK] = 0;
        parameters.value[PARAM_INTERVAL] = 20;
        parameters.value[PARAM_GRAINS] = 32;
        parameters.value[PARAM_LENGTH] = 500;
        parameters.value[PARAM_ATTACK] = 10;
        parameters.value[PARAM_RELEASE] = 10;
        parameters.value[PARAM_REVERB] = 0;
        parameters.value[PARAM_LOWPASS] = 8000;
        dry_jump = render_maximum_jump(&entry, &parameters, 4);

        parameters.value[PARAM_LOWPASS] = 3000;
        filtered_jump = render_maximum_jump(&entry, &parameters, 4);
        printf("sample %-31s source=%3d normal=%3d dry=%3d LPF3k=%3d\n",
               entry.name, maximum_source_jump(entry.pcm, entry.length),
               normal_jump, dry_jump, filtered_jump);
        assert(normal_jump <= 24);
        assert(dry_jump <= 127 && filtered_jump <= dry_jump);
    }
    free(bytes);
    return 0;
}
