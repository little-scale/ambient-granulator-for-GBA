#include "parameters.h"
#include "text_format.h"

#include <stdlib.h>

const ParameterDefinition parameter_definitions[PARAM_COUNT] = {
    { "RANGE",    -128,  128,  1,  16,  1, 0 },
    { "PITCH",     -24,   24,  1,  12,  2, 0 },
    { "FINE",      -100,  100,  1,  10,  3, 0 },
    { "P DEV",       0,   12,  1,   4,  4, 0 },
    { "F DEV",       0,  100,  1,  10,  5, 0 },
    { "CLOCK",       0,    1,  1,   1,  7, 0 },
    { "BPM",        40,  240,  1,  10,  8, 0 },
    { "DIV",         0,    3,  1,   1,  9, 0 },
    { "INTERVAL",   20, 1000, 10, 100, 10, 0 },
    { "JITTER",      0,  100,  5,  20, 11, 0 },
    { "GRAINS",      1,   32,  1,   4, 13, 0 },
    { "LENGTH",     20,  500, 10, 100,  1, 1 },
    { "ATTACK",      0,   50,  5,  20,  2, 1 },
    { "RELEASE",     0,   50,  5,  20,  3, 1 },
    { "GAIN",      -24,   18,  1,   6,  4, 1 },
    { "VOL",         0,  100,  1,  10,  5, 1 },
    { "PAN",      -100,  100,  1,  10,  6, 1 },
    { "P DEV",       0,  100,  1,  10,  7, 1 },
    { "REV",         0,  100,  1,  10,  9, 1 },
    { "FEEDBACK",    0,  999,  1,  10, 10, 1 },
    { "SIZE",        0,  100,  1,  10, 11, 1 },
    { "DAMP",        0,  100,  1,  10, 12, 1 },
    { "FREEZE",      0,    1,  1,   1, 13, 1 },
    { "HPF",         0, 4000, 50, 500, 14, 1 },
    { "LPF",       200, 8000, 50, 500, 15, 1 },
};

const int parameter_divisions[4] = { 8, 16, 32, 64 };

void parameters_reset(ParameterState *state)
{
#ifdef AMBIENT_MAX_LOAD_PROFILE
#ifdef AMBIENT_MAX_LOAD_NO_FILTERS
#define MAX_LOAD_HPF 0
#define MAX_LOAD_LPF 8000
#elif defined(AMBIENT_MAX_LOAD_LPF_ONLY)
#define MAX_LOAD_HPF 0
#define MAX_LOAD_LPF 2000
#else
#define MAX_LOAD_HPF 50
#define MAX_LOAD_LPF 2000
#endif
    static const int defaults[PARAM_COUNT] = {
        24, 0, 0, 12, 100, 0, 98, 0, 20, 0, 32,
        500, 10, 10, 0, 100, 0, 100,
        100, 990, 100, 10, 0, MAX_LOAD_HPF, MAX_LOAD_LPF
    };
#undef MAX_LOAD_HPF
#undef MAX_LOAD_LPF
#else
    static const int defaults[PARAM_COUNT] = {
        24, 0, 0, 0, 0, 1, 98, 0, 120, 20, 8,
        420, 50, 50, 0, 100, 0, 100,
        70, 990, 100, 10, 0, 0, 4000
    };
#endif
    int index;
    for (index = 0; index < PARAM_COUNT; ++index)
        state->value[index] = defaults[index];
}

void parameters_nudge(ParameterState *state, ParameterId id, int amount)
{
    int value = state->value[id] + amount;
    if (value < parameter_definitions[id].minimum)
        value = parameter_definitions[id].minimum;
    if (value > parameter_definitions[id].maximum)
        value = parameter_definitions[id].maximum;
    state->value[id] = value;
}

ParameterId parameters_move(ParameterId current, int horizontal, int vertical)
{
    int current_column = parameter_definitions[current].column;
    int current_row = parameter_definitions[current].row;
    int target_column = current_column + horizontal;
    int best = -1;
    int best_distance = 1000;
    int index;

    if (horizontal != 0 && target_column >= 0 && target_column <= 1) {
        for (index = 0; index < PARAM_COUNT; ++index) {
            int distance;
            if (parameter_definitions[index].column != target_column)
                continue;
            distance = abs((int)parameter_definitions[index].row - current_row);
            if (distance < best_distance) {
                best = index;
                best_distance = distance;
            }
        }
        if (best >= 0)
            current = (ParameterId)best;
    }

    if (vertical != 0) {
        current_column = parameter_definitions[current].column;
        current_row = parameter_definitions[current].row;
        best = -1;
        best_distance = 1000;
        for (index = 0; index < PARAM_COUNT; ++index) {
            int delta;
            if (parameter_definitions[index].column != current_column)
                continue;
            delta = (int)parameter_definitions[index].row - current_row;
            if ((vertical > 0 && delta > 0) || (vertical < 0 && delta < 0)) {
                int distance = abs(delta);
                if (distance < best_distance) {
                    best = index;
                    best_distance = distance;
                }
            }
        }
        if (best < 0) {
            int extreme = vertical > 0 ? 1000 : -1;
            for (index = 0; index < PARAM_COUNT; ++index) {
                int row = parameter_definitions[index].row;
                if (parameter_definitions[index].column != current_column)
                    continue;
                if ((vertical > 0 && row < extreme)
                        || (vertical < 0 && row > extreme)) {
                    best = index;
                    extreme = row;
                }
            }
        }
        if (best >= 0)
            current = (ParameterId)best;
    }
    return current;
}

void parameters_format(const ParameterState *state, ParameterId id,
                       char *text, size_t size)
{
    TextBuffer output;
    int value = state->value[id];
    text_init(&output, text, size);
    if (id == PARAM_RANGE && value > 0) {
        text_append(&output, "+-");
        text_append_uint(&output, (unsigned int)value, 1);
    } else if (id == PARAM_CLOCK) {
        text_append(&output, value ? "SYNC" : "FREE");
    } else if (id == PARAM_DIVISION) {
        text_append(&output, "1/");
        text_append_uint(&output, (unsigned int)parameter_divisions[value], 1);
    } else if (id == PARAM_REVERB_FREEZE) {
        text_append(&output, value ? "ON" : "OFF");
    }
    else if ((id == PARAM_HIGHPASS && value == 0)
            || (id == PARAM_LOWPASS && value >= 8000)) {
        text_append(&output, "OFF");
    } else if (id == PARAM_REVERB_FEEDBACK) {
        text_append_uint(&output, (unsigned int)(value / 10), 1);
        text_append_char(&output, '.');
        text_append_uint(&output, (unsigned int)(value % 10), 1);
        text_append_char(&output, '%');
    } else if (id == PARAM_GAIN) {
        text_append_int(&output, value, 1, 1);
        text_append(&output, "DB");
    } else if (id == PARAM_PITCH || id == PARAM_PITCH_DEVIATION) {
        text_append_int(&output, value, 1, 1);
        text_append(&output, "ST");
    } else if (id == PARAM_FINE) {
        text_append_int(&output, value, 1, 1);
        text_append(&output, "CT");
    } else if (id == PARAM_FINE_DEVIATION) {
        text_append_uint(&output, (unsigned int)value, 1);
        text_append(&output, "CT");
    } else if (id == PARAM_INTERVAL || id == PARAM_LENGTH) {
        text_append_uint(&output, (unsigned int)value, 1);
        text_append(&output, "MS");
    } else if (id == PARAM_RANGE) {
        text_append_int(&output, value, 0, 1);
        text_append(&output, "PX");
    }
    else if (id == PARAM_JITTER || id == PARAM_ATTACK || id == PARAM_RELEASE
            || id == PARAM_VOLUME || id == PARAM_PAN
            || id == PARAM_PAN_DEVIATION || id == PARAM_REVERB
            || id == PARAM_REVERB_SIZE
            || id == PARAM_REVERB_DAMP) {
        text_append_int(&output, value, 0, 1);
        text_append_char(&output, '%');
    } else if (id == PARAM_HIGHPASS || id == PARAM_LOWPASS) {
        text_append_uint(&output, (unsigned int)value, 1);
        text_append(&output, "HZ");
    } else {
        text_append_int(&output, value, 0, 1);
    }
}
