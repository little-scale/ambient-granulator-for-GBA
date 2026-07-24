#ifndef AMBGRANULAR_FONT_H
#define AMBGRANULAR_FONT_H

#include <gba_types.h>

enum {
    FONT_GLYPH_WIDTH = 5,
    FONT_GLYPH_HEIGHT = 7,
    FONT_ADVANCE = 6
};

void font_draw_char(int x, int y, char character, u8 color);
void font_draw_text(int x, int y, const char *text, u8 color);
int font_text_width(const char *text);

#endif

