#include "parameters.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ParameterState state;
    char text[16];

    parameters_reset(&state);
    assert(state.value[PARAM_CLOCK] == 1);
    assert(state.value[PARAM_FINE] == 0);
    assert(state.value[PARAM_FINE_DEVIATION] == 0);
    assert(state.value[PARAM_BPM] == 98);
    assert(state.value[PARAM_GRAINS] == 8);
    assert(state.value[PARAM_REVERB] == 70);
    assert(state.value[PARAM_REVERB_FEEDBACK] == 990);
    assert(state.value[PARAM_LOWPASS] == 4000);

    parameters_nudge(&state, PARAM_BPM, 1000);
    assert(state.value[PARAM_BPM] == 240);
    parameters_nudge(&state, PARAM_BPM, -1000);
    assert(state.value[PARAM_BPM] == 40);
    assert(parameters_move(PARAM_RANGE, 1, 0) == PARAM_LENGTH);
    assert(parameters_move(PARAM_RANGE, 0, -1) == PARAM_GRAINS);
    assert(parameters_move(PARAM_LOWPASS, 0, 1) == PARAM_LENGTH);

    parameters_reset(&state);
    parameters_format(&state, PARAM_CLOCK, text, sizeof(text));
    assert(strcmp(text, "SYNC") == 0);
    parameters_format(&state, PARAM_DIVISION, text, sizeof(text));
    assert(strcmp(text, "1/8") == 0);
    parameters_format(&state, PARAM_REVERB_FREEZE, text, sizeof(text));
    assert(strcmp(text, "OFF") == 0);
    parameters_format(&state, PARAM_LOWPASS, text, sizeof(text));
    assert(strcmp(text, "4000HZ") == 0);
    state.value[PARAM_REVERB_FEEDBACK] = 999;
    parameters_format(&state, PARAM_REVERB_FEEDBACK, text, sizeof(text));
    assert(strcmp(text, "99.9%") == 0);
    state.value[PARAM_FINE] = -100;
    parameters_format(&state, PARAM_FINE, text, sizeof(text));
    assert(strcmp(text, "-100CT") == 0);
    state.value[PARAM_FINE_DEVIATION] = 75;
    parameters_format(&state, PARAM_FINE_DEVIATION, text, sizeof(text));
    assert(strcmp(text, "75CT") == 0);
    puts("parameter defaults, bounds, navigation and formatting passed");
    return 0;
}
