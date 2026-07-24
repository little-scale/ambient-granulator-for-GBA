#ifndef AMBGRANULAR_PARAMETERS_H
#define AMBGRANULAR_PARAMETERS_H

#include <stddef.h>

typedef enum {
    PARAM_RANGE,
    PARAM_PITCH,
    PARAM_FINE,
    PARAM_PITCH_DEVIATION,
    PARAM_FINE_DEVIATION,
    PARAM_CLOCK,
    PARAM_BPM,
    PARAM_DIVISION,
    PARAM_INTERVAL,
    PARAM_JITTER,
    PARAM_GRAINS,
    PARAM_LENGTH,
    PARAM_ATTACK,
    PARAM_RELEASE,
    PARAM_GAIN,
    PARAM_VOLUME,
    PARAM_PAN,
    PARAM_PAN_DEVIATION,
    PARAM_REVERB,
    PARAM_REVERB_FEEDBACK,
    PARAM_REVERB_SIZE,
    PARAM_REVERB_DAMP,
    PARAM_REVERB_FREEZE,
    PARAM_HIGHPASS,
    PARAM_LOWPASS,
    PARAM_COUNT
} ParameterId;

typedef struct {
    const char *name;
    int minimum;
    int maximum;
    int step;
    int coarse_step;
    unsigned char row;
    unsigned char column;
} ParameterDefinition;

typedef struct {
    int value[PARAM_COUNT];
} ParameterState;

extern const ParameterDefinition parameter_definitions[PARAM_COUNT];
extern const int parameter_divisions[4];

void parameters_reset(ParameterState *state);
void parameters_nudge(ParameterState *state, ParameterId id, int amount);
ParameterId parameters_move(ParameterId current, int horizontal, int vertical);
void parameters_format(const ParameterState *state, ParameterId id,
                       char *text, size_t size);

#endif
