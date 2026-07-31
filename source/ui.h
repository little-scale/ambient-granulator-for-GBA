#ifndef AMBGRANULAR_UI_H
#define AMBGRANULAR_UI_H

#include "kiosk.h"
#include "parameters.h"
#include "sample_bank.h"

#include <gba_types.h>

typedef enum {
    UI_VIEW_PERFORMANCE,
    UI_VIEW_EDIT,
    UI_VIEW_BROWSER,
    UI_VIEW_BANK_ERROR
} UiView;

typedef struct {
    u8 x;
    u8 ttl;
} UiGrainMarker;

typedef struct {
    UiView view;
    UiView return_view;
    ParameterState parameters;
    ParameterId selected_parameter;
    SampleBank bank;
    SampleBankEntry sample;
    u32 sample_index;
    u32 browser_index;
    int position;
    int b_used;
    u8 dirty;
    u8 status_frames;
    u16 auto_freeze_delay_frames;
    u32 auto_freeze_target_grains;
    KioskState kiosk;
    UiGrainMarker markers[16];
} UiState;

int ui_init(UiState *state, u32 random_seed);
void ui_handle_input(UiState *state, u16 held, u16 pressed,
                     u16 released, u16 repeated);
void ui_tick(UiState *state);
void ui_render(const UiState *state);

#endif
