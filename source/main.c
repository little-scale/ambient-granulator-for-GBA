#include "audio.h"
#include "gfx.h"
#include "ui.h"

#include <gba_input.h>
#include <gba_interrupt.h>

static volatile u32 video_frames;
static UiState ui_state EWRAM_DATA;

static void vblank_irq(void)
{
    ++video_frames;
}

static void commit_frame(void)
{
    int segment;
    for (segment = 0; segment < 4; ++segment) {
        gfx_commit_segment(segment);
        audio_service();
    }
}

int main(void)
{
    u32 displayed_frame = 0;
    int frame_pending = 1;
    int bank_valid;

    irqInit();
    irqSet(IRQ_VBLANK, vblank_irq);
    irqEnable(IRQ_VBLANK);

    gfx_init();
    setRepeat(18, 4);
    bank_valid = ui_init(&ui_state);
    audio_init(bank_valid ? ui_state.sample.pcm : 0,
               bank_valid ? ui_state.sample.length : 0,
               &ui_state.parameters);
    ui_render(&ui_state);
    ui_state.dirty = 0;
    commit_frame();

    for (;;) {
        audio_service();

        if (video_frames != displayed_frame) {
            displayed_frame = video_frames;
            if (frame_pending) {
                gfx_present();
                frame_pending = 0;
            }
            audio_service();
            scanKeys();
            ui_handle_input(&ui_state, keysHeld(), keysDown(), keysUp(),
                            keysDownRepeat());
            ui_tick(&ui_state);
            if (ui_state.dirty) {
                audio_service();
                ui_render(&ui_state);
                audio_service();
                commit_frame();
                ui_state.dirty = 0;
                frame_pending = 1;
            }
            audio_service();
        }
    }
}
