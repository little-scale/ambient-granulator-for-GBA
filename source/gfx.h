#ifndef AMBGRANULAR_GFX_H
#define AMBGRANULAR_GFX_H

#include <gba_types.h>

enum {
    SCREEN_WIDTH = 240,
    SCREEN_HEIGHT = 160,
    COLOR_BLACK = 0,
    COLOR_WHITE = 1
};

void gfx_init(void);
void gfx_commit(void);
void gfx_commit_segment(int segment);
void gfx_present(void);
void gfx_clear(u8 color);
void gfx_pixel(int x, int y, u8 color);
void gfx_xor_pixel(int x, int y);
void gfx_fill_rect(int x, int y, int width, int height, u8 color);
void gfx_rect(int x, int y, int width, int height, u8 color);
void gfx_hline(int x, int y, int width, u8 color);
void gfx_vline(int x, int y, int height, u8 color);

#endif
