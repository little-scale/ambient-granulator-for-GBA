#include "ui.h"

#include "audio.h"
#include "font.h"
#include "gfx.h"
#include "text_format.h"

#include <gba_input.h>
#include <string.h>

enum {
    MARKER_FRAMES = 18,
    BROWSER_PAGE_ROWS = 10,
    PERFORMANCE_WAVE_ZERO = 74,
    PERFORMANCE_WAVE_AMPLITUDE = 58,
    PERFORMANCE_CONTENT_BOTTOM = 139,
    PERFORMANCE_STATUS_TOP = 142,
    PERFORMANCE_STATUS_TEXT_Y = 144
};

static int waveform_top(const SampleBankEntry *sample, int x,
                        int zero, int amplitude)
{
    return zero - (int)sample->maximums[x] * amplitude / 127;
}

static int waveform_bottom(const SampleBankEntry *sample, int x,
                           int zero, int amplitude)
{
    return zero - (int)sample->minimums[x] * amplitude / 127;
}

static void draw_waveform(const SampleBankEntry *sample, int zero,
                          int amplitude, u8 color)
{
    int x;
    for (x = 0; x < SAMPLE_BANK_WAVEFORM_COLUMNS; ++x) {
        int top = waveform_top(sample, x, zero, amplitude);
        int bottom = waveform_bottom(sample, x, zero, amplitude);
        if (bottom < top) {
            int swap = top;
            top = bottom;
            bottom = swap;
        }
        gfx_vline(x, top, bottom - top + 1, color);
        if (x == 59 || x == 119 || x == 179)
            audio_service();
    }
}

static void draw_dashed_boundary(int x, int top, int bottom)
{
    int y;
    for (y = top; y <= bottom; y += 4) {
        gfx_vline(x, y, 2, COLOR_WHITE);
        gfx_vline(x + 1, y, 2, COLOR_WHITE);
    }
}

static void draw_markers(const UiState *state, int zero, int amplitude)
{
    int marker_index;
    for (marker_index = 0; marker_index < 16; ++marker_index) {
        const UiGrainMarker *marker = &state->markers[marker_index];
        int x;
        if (marker->ttl == 0)
            continue;
        for (x = (int)marker->x - 2; x <= (int)marker->x + 1; ++x) {
            int top;
            int bottom;
            int y;
            if (x < 0 || x >= SCREEN_WIDTH)
                continue;
            top = waveform_top(&state->sample, x, zero, amplitude);
            bottom = waveform_bottom(&state->sample, x, zero, amplitude);
            if (bottom < top) {
                int swap = top;
                top = bottom;
                bottom = swap;
            }
            for (y = top; y <= bottom; ++y)
                gfx_xor_pixel(x, y);
        }
        audio_service();
    }
}

static void render_performance(const UiState *state)
{
    char status[41];
    TextBuffer text;
    int range = state->parameters.value[PARAM_RANGE];
    int left;
    int right;
    unsigned long underruns = audio_underruns > 999 ? 999 : audio_underruns;
    unsigned long load = (unsigned long)(worst_mix_cycles * 100u
                                         / AUDIO_BLOCK_CYCLES);

    if (load > 99)
        load = 99;

    gfx_clear(COLOR_BLACK);
    audio_service();
    draw_waveform(&state->sample, PERFORMANCE_WAVE_ZERO,
                  PERFORMANCE_WAVE_AMPLITUDE, COLOR_WHITE);
    audio_service();
    draw_markers(state, PERFORMANCE_WAVE_ZERO,
                 PERFORMANCE_WAVE_AMPLITUDE);

    if (range >= 0) {
        left = state->position - range;
        right = state->position + range;
    } else {
        left = state->position;
        right = state->position - range;
    }
    if (left >= 0 && left < SCREEN_WIDTH)
        draw_dashed_boundary(left, 6, PERFORMANCE_CONTENT_BOTTOM - 1);
    if (right >= 0 && right < SCREEN_WIDTH)
        draw_dashed_boundary(right, 6, PERFORMANCE_CONTENT_BOTTOM - 1);
    gfx_vline(state->position, 4, PERFORMANCE_CONTENT_BOTTOM - 3,
              COLOR_WHITE);
    audio_service();

    gfx_fill_rect(0, PERFORMANCE_STATUS_TOP, SCREEN_WIDTH,
                  SCREEN_HEIGHT - PERFORMANCE_STATUS_TOP, COLOR_BLACK);
    text_init(&text, status, sizeof(status));
    text_append_field(&text, state->sample.name, 8, 8);
    text_append(&text, " B");
    text_append_uint(&text, (unsigned int)state->parameters.value[PARAM_BPM], 3);
    text_append_char(&text, '/');
    text_append_uint(&text, (unsigned int)parameter_divisions[
                     state->parameters.value[PARAM_DIVISION]], 1);
    if (parameter_divisions[state->parameters.value[PARAM_DIVISION]] < 10)
        text_append_char(&text, ' ');
    text_append(&text, " P");
    text_append_int(&text, state->parameters.value[PARAM_PITCH], 1, 2);
    text_append(&text, " F");
    text_append_int(&text, state->parameters.value[PARAM_FINE], 1, 3);
    text_append_char(&text, ' ');
    text_append(&text, state->parameters.value[PARAM_REVERB_FREEZE]
                ? "FZ" : "--");
    text_append(&text, " U");
    text_append_uint(&text, (unsigned int)underruns, 3);
    text_append(&text, " C");
    text_append_uint(&text, (unsigned int)load, 2);
    font_draw_text(2, PERFORMANCE_STATUS_TEXT_Y, status, COLOR_WHITE);
    audio_service();
}

static void draw_parameter(const UiState *state, ParameterId id)
{
    const ParameterDefinition *definition = &parameter_definitions[id];
    int column_x = definition->column ? 123 : 3;
    int value_x = column_x + 72;
    int y = 12 + definition->row * 7;
    char value[12];
    int text_width;
    u8 text_color = COLOR_BLACK;

    parameters_format(&state->parameters, id, value, sizeof(value));
    text_width = font_text_width(value);
    font_draw_text(column_x, y, definition->name, COLOR_BLACK);
    if (id == state->selected_parameter) {
        gfx_fill_rect(value_x, y - 1, 43, 9, COLOR_BLACK);
        text_color = COLOR_WHITE;
    }
    font_draw_text(value_x + 40 - text_width, y, value, text_color);
}

static void render_edit(const UiState *state)
{
    int id;

    gfx_clear(COLOR_WHITE);
    audio_service();
    font_draw_text(3, 2, "GRAIN / CLOCK", COLOR_BLACK);
    font_draw_text(123, 2, "VOICE / FX", COLOR_BLACK);
    gfx_hline(3, 9, 114, COLOR_BLACK);
    gfx_hline(123, 9, 114, COLOR_BLACK);
    for (id = 0; id < PARAM_COUNT; ++id) {
        draw_parameter(state, (ParameterId)id);
        if ((id & 3) == 3)
            audio_service();
    }

    gfx_fill_rect(0, 128, SCREEN_WIDTH, 32, COLOR_BLACK);
    draw_waveform(&state->sample, 144, 13, COLOR_WHITE);
    gfx_vline(state->position, 130, 28, COLOR_WHITE);
}

static void render_browser(const UiState *state)
{
    char heading[32];
    TextBuffer heading_text;
    u32 page_start = (state->browser_index / BROWSER_PAGE_ROWS)
                   * BROWSER_PAGE_ROWS;
    u32 row;

    gfx_clear(COLOR_WHITE);
    audio_service();
    text_init(&heading_text, heading, sizeof(heading));
    text_append(&heading_text, "SAMPLES ");
    text_append_uint(&heading_text, (unsigned int)(state->browser_index + 1), 2);
    text_append_char(&heading_text, '/');
    text_append_uint(&heading_text, (unsigned int)state->bank.count, 2);
    font_draw_text(4, 4, heading, COLOR_BLACK);
    gfx_hline(4, 13, 232, COLOR_BLACK);
    for (row = 0; row < BROWSER_PAGE_ROWS; ++row) {
        u32 index = page_start + row;
        SampleBankEntry entry;
        char line[48];
        TextBuffer line_text;
        int y = 18 + (int)row * 13;
        u8 color = COLOR_BLACK;
        if (index >= state->bank.count)
            break;
        if (!sample_bank_get(&state->bank, index, &entry))
            continue;
        text_init(&line_text, line, sizeof(line));
        text_append_char(&line_text,
                         index == state->sample_index ? 'C' : ' ');
        text_append_char(&line_text, ' ');
        text_append_uint(&line_text, (unsigned int)(index + 1), 2);
        text_append_char(&line_text, ' ');
        text_append_field(&line_text, entry.name, 19, 19);
        text_append_char(&line_text, ' ');
        text_append_uint(&line_text,
                         (unsigned int)(entry.length / SAMPLE_BANK_RATE), 1);
        text_append_char(&line_text, '.');
        text_append_uint(&line_text,
                         (unsigned int)((entry.length % SAMPLE_BANK_RATE) * 10
                         / SAMPLE_BANK_RATE), 1);
        text_append_char(&line_text, 'S');
        if (index == state->browser_index) {
            gfx_fill_rect(2, y - 2, 236, 11, COLOR_BLACK);
            color = COLOR_WHITE;
        }
        font_draw_text(4, y, line, color);
        audio_service();
    }
    font_draw_text(4, 151, "A/B LOAD   SELECT CANCEL", COLOR_BLACK);
}

static void render_bank_error(void)
{
    gfx_clear(COLOR_WHITE);
    gfx_fill_rect(0, 0, SCREEN_WIDTH, 16, COLOR_BLACK);
    font_draw_text(4, 4, "SAMPLE BANK ERROR", COLOR_WHITE);
    font_draw_text(4, 28, "ROM SAMPLE BANK IS INVALID", COLOR_BLACK);
    font_draw_text(4, 40, "REBUILD THE ROM", COLOR_BLACK);
}

static void toggle_freeze(UiState *state)
{
    state->parameters.value[PARAM_REVERB_FREEZE] ^= 1;
    audio_set_parameters(&state->parameters);
    state->dirty = 1;
}

static void open_browser(UiState *state)
{
    audio_set_gate(0, state->position);
    state->return_view = state->view;
    state->browser_index = state->sample_index;
    state->view = UI_VIEW_BROWSER;
    state->dirty = 1;
}

static void load_browser_sample(UiState *state)
{
    SampleBankEntry entry;
    if (!sample_bank_get(&state->bank, state->browser_index, &entry))
        return;
    state->sample = entry;
    state->sample_index = state->browser_index;
    audio_set_sample(state->sample.pcm, state->sample.length);
    state->view = state->return_view;
    state->dirty = 1;
}

static int direction_edit_amount(ParameterId id, u16 directions)
{
    const ParameterDefinition *definition = &parameter_definitions[id];
    int amount = 0;

    if (directions & KEY_LEFT)
        amount -= definition->step;
    if (directions & KEY_RIGHT)
        amount += definition->step;
    if (directions & KEY_DOWN)
        amount -= definition->coarse_step;
    if (directions & KEY_UP)
        amount += definition->coarse_step;
    return amount;
}

static void edit_parameter(UiState *state, ParameterId id, u16 directions)
{
    int previous = state->parameters.value[id];
    parameters_nudge(&state->parameters, id,
                     direction_edit_amount(id, directions));
    if (state->parameters.value[id] != previous) {
        audio_set_parameters(&state->parameters);
        state->dirty = 1;
    }
}

int ui_init(UiState *state)
{
    memset(state, 0, sizeof(*state));
    parameters_reset(&state->parameters);
    state->selected_parameter = PARAM_RANGE;
    state->position = 120;
    state->view = UI_VIEW_PERFORMANCE;
    state->return_view = UI_VIEW_PERFORMANCE;
    state->dirty = 1;
    if (!sample_bank_open_embedded(&state->bank)
            || !sample_bank_get(&state->bank, 0, &state->sample)) {
        state->view = UI_VIEW_BANK_ERROR;
        return 0;
    }
    return 1;
}

void ui_handle_input(UiState *state, u16 held, u16 pressed,
                     u16 released, u16 repeated)
{
    u16 directions = repeated & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN);

    if (state->view == UI_VIEW_BANK_ERROR)
        return;
    if (state->view == UI_VIEW_BROWSER) {
        if (pressed & KEY_SELECT) {
            state->view = state->return_view;
            state->dirty = 1;
            return;
        }
        if ((pressed & (KEY_A | KEY_B)) != 0) {
            load_browser_sample(state);
            return;
        }
        {
            u32 previous_index = state->browser_index;
            if ((repeated & KEY_UP) && state->browser_index > 0)
                --state->browser_index;
            if ((repeated & KEY_DOWN)
                    && state->browser_index + 1 < state->bank.count)
                ++state->browser_index;
            if (repeated & KEY_LEFT) {
                if (state->browser_index >= BROWSER_PAGE_ROWS)
                    state->browser_index -= BROWSER_PAGE_ROWS;
                else
                    state->browser_index = 0;
            }
            if (repeated & KEY_RIGHT) {
                state->browser_index += BROWSER_PAGE_ROWS;
                if (state->browser_index >= state->bank.count)
                    state->browser_index = state->bank.count - 1;
            }
            if (state->browser_index != previous_index)
                state->dirty = 1;
        }
        return;
    }

    if (pressed & KEY_L)
        toggle_freeze(state);
    if (pressed & KEY_SELECT) {
        open_browser(state);
        return;
    }
    if (pressed & KEY_START) {
        audio_set_gate(0, state->position);
        state->view = state->view == UI_VIEW_PERFORMANCE
            ? UI_VIEW_EDIT : UI_VIEW_PERFORMANCE;
        state->dirty = 1;
        return;
    }
    if (pressed & KEY_B)
        state->b_used = 0;

    if (state->view == UI_VIEW_PERFORMANCE) {
        int previous_position = state->position;
        if ((held & KEY_A) && directions) {
            edit_parameter(state, PARAM_RANGE, directions);
        } else if ((held & KEY_R) && directions) {
            edit_parameter(state, PARAM_PITCH, directions);
        } else {
            if (directions & KEY_LEFT)
                --state->position;
            if (directions & KEY_RIGHT)
                ++state->position;
            if (directions & KEY_UP)
                state->position -= 8;
            if (directions & KEY_DOWN)
                state->position += 8;
        }
        if (state->position < 0)
            state->position = 0;
        if (state->position >= SCREEN_WIDTH)
            state->position = SCREEN_WIDTH - 1;
        if (state->position != previous_position)
            state->dirty = 1;
        audio_set_gate((held & KEY_A) != 0, state->position);
    } else {
        audio_set_gate(0, state->position);
        if ((held & KEY_B) && directions) {
            edit_parameter(state, state->selected_parameter, directions);
            state->b_used = 1;
        } else if (directions) {
            ParameterId previous_parameter = state->selected_parameter;
            state->selected_parameter = parameters_move(
                state->selected_parameter,
                (directions & KEY_RIGHT) != 0
                    ? 1 : ((directions & KEY_LEFT) != 0 ? -1 : 0),
                (directions & KEY_DOWN) != 0
                    ? 1 : ((directions & KEY_UP) != 0 ? -1 : 0));
            if (state->selected_parameter != previous_parameter)
                state->dirty = 1;
        }
    }

    if ((released & KEY_B) && !state->b_used)
        audio_trigger_burst(state->position);
}

void ui_tick(UiState *state)
{
    int index;
    u8 marker_x;

#ifdef AMBIENT_FIFO_CONTINUITY_PROFILE
    /* Exercise FIFO handoffs while Mode 4 screen DMA runs every frame. */
    state->dirty = 1;
#endif

    for (index = 0; index < 16; ++index)
        if (state->markers[index].ttl > 0) {
            --state->markers[index].ttl;
            if (state->markers[index].ttl == 0
                    && state->view == UI_VIEW_PERFORMANCE)
                state->dirty = 1;
        }
    while (audio_pop_marker(&marker_x)) {
        int slot = -1;
        int oldest = 0;
        for (index = 0; index < 16; ++index) {
            if (state->markers[index].ttl == 0) {
                slot = index;
                break;
            }
            if (state->markers[index].ttl < state->markers[oldest].ttl)
                oldest = index;
        }
        if (slot < 0)
            slot = oldest;
        state->markers[slot].x = marker_x;
        state->markers[slot].ttl = MARKER_FRAMES;
        if (state->view == UI_VIEW_PERFORMANCE)
            state->dirty = 1;
    }
    ++state->status_frames;
    if (state->status_frames >= 60) {
        state->status_frames = 0;
        if (state->view == UI_VIEW_PERFORMANCE)
            state->dirty = 1;
    }
}

void ui_render(const UiState *state)
{
    if (state->view == UI_VIEW_PERFORMANCE)
        render_performance(state);
    else if (state->view == UI_VIEW_EDIT)
        render_edit(state);
    else if (state->view == UI_VIEW_BROWSER)
        render_browser(state);
    else
        render_bank_error();
}
