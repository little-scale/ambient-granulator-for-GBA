#include "gfx.h"

#include <gba_base.h>
#include <gba_dma.h>
#include <gba_video.h>

#define MODE4_PAGE_0 ((vu16 *)0x06000000)
#define MODE4_PAGE_1 ((vu16 *)0x0600a000)

static vu16 *draw_page;
static u8 frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT] EWRAM_DATA ALIGN(4);
static u16 clear_value IWRAM_DATA ALIGN(4);

void gfx_init(void)
{
    BG_PALETTE[COLOR_BLACK] = RGB5(0, 0, 0);
    BG_PALETTE[COLOR_WHITE] = RGB5(31, 31, 31);
    REG_DISPCNT = MODE_4 | BG2_ON;
    draw_page = MODE4_PAGE_1;
}

void gfx_present(void)
{
    if (draw_page == MODE4_PAGE_1) {
        REG_DISPCNT = MODE_4 | BG2_ON | BACKBUFFER;
        draw_page = MODE4_PAGE_0;
    } else {
        REG_DISPCNT = MODE_4 | BG2_ON;
        draw_page = MODE4_PAGE_1;
    }
}

void gfx_commit(void)
{
    REG_DMA3CNT = 0;
    REG_DMA3SAD = (u32)frame_buffer;
    REG_DMA3DAD = (u32)draw_page;
    REG_DMA3CNT = DMA_SRC_INC | DMA_DST_INC | DMA16 |
                  DMA_ENABLE | ((SCREEN_WIDTH * SCREEN_HEIGHT) / 2);
}

void gfx_commit_segment(int segment)
{
    enum {
        SEGMENTS = 4,
        BYTES_PER_SEGMENT = (SCREEN_WIDTH * SCREEN_HEIGHT) / SEGMENTS,
        HALFWORDS_PER_SEGMENT = BYTES_PER_SEGMENT / 2
    };
    unsigned int offset;

    if ((unsigned int)segment >= SEGMENTS)
        return;
    offset = (unsigned int)segment * BYTES_PER_SEGMENT;
    REG_DMA3CNT = 0;
    REG_DMA3SAD = (u32)(frame_buffer + offset);
    REG_DMA3DAD = (u32)((u8 *)draw_page + offset);
    REG_DMA3CNT = DMA_SRC_INC | DMA_DST_INC | DMA16 |
                  DMA_ENABLE | HALFWORDS_PER_SEGMENT;
}

void gfx_clear(u8 color)
{
    clear_value = (u16)color | ((u16)color << 8);
    REG_DMA3CNT = 0;
    REG_DMA3SAD = (u32)&clear_value;
    REG_DMA3DAD = (u32)frame_buffer;
    REG_DMA3CNT = DMA_SRC_FIXED | DMA_DST_INC | DMA16 |
                  DMA_ENABLE | ((SCREEN_WIDTH * SCREEN_HEIGHT) / 2);
}

void gfx_pixel(int x, int y, u8 color)
{
    if ((unsigned int)x >= SCREEN_WIDTH || (unsigned int)y >= SCREEN_HEIGHT)
        return;

    frame_buffer[y * SCREEN_WIDTH + x] = color;
}

void gfx_xor_pixel(int x, int y)
{
    if ((unsigned int)x >= SCREEN_WIDTH || (unsigned int)y >= SCREEN_HEIGHT)
        return;
    frame_buffer[y * SCREEN_WIDTH + x] ^= 1u;
}

void gfx_fill_rect(int x, int y, int width, int height, u8 color)
{
    int row;
    int column;
    u8 *destination;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > SCREEN_WIDTH)
        width = SCREEN_WIDTH - x;
    if (y + height > SCREEN_HEIGHT)
        height = SCREEN_HEIGHT - y;
    if (width <= 0 || height <= 0)
        return;
    for (row = 0; row < height; ++row) {
        destination = &frame_buffer[(y + row) * SCREEN_WIDTH + x];
        for (column = 0; column < width; ++column)
            destination[column] = color;
    }
}

void gfx_vline(int x, int y, int height, u8 color)
{
    int row;
    u8 *destination;

    if ((unsigned int)x >= SCREEN_WIDTH)
        return;
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y + height > SCREEN_HEIGHT)
        height = SCREEN_HEIGHT - y;
    if (height <= 0)
        return;
    destination = &frame_buffer[y * SCREEN_WIDTH + x];
    for (row = 0; row < height; ++row) {
        *destination = color;
        destination += SCREEN_WIDTH;
    }
}

void gfx_hline(int x, int y, int width, u8 color)
{
    int column;

    for (column = 0; column < width; ++column)
        gfx_pixel(x + column, y, color);
}

void gfx_rect(int x, int y, int width, int height, u8 color)
{
    int row;

    gfx_hline(x, y, width, color);
    gfx_hline(x, y + height - 1, width, color);
    for (row = 1; row < height - 1; ++row) {
        gfx_pixel(x, y + row, color);
        gfx_pixel(x + width - 1, y + row, color);
    }
}
