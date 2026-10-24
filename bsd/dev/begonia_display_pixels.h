/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pixel contract follows mouseos/edk2-mtk SimpleFbDxe at 1982d888.
 * Call only with all four reserved framebuffer surfaces mapped writable.
 */
#ifndef BEGONIA_DISPLAY_PIXELS_H
#define BEGONIA_DISPLAY_PIXELS_H
#define BEGONIA_DISPLAY_WIDTH 800U
#define BEGONIA_DISPLAY_HEIGHT 1280U
#define BEGONIA_DISPLAY_BUFFERS 4U
#define BEGONIA_DISPLAY_PIXELS (BEGONIA_DISPLAY_WIDTH * BEGONIA_DISPLAY_HEIGHT)
#define BEGONIA_DISPLAY_BYTES (BEGONIA_DISPLAY_PIXELS * 4UL * BEGONIA_DISPLAY_BUFFERS)
static inline void begonia_display_paint(volatile uint32_t *pixels)
{
    for (unsigned buffer = 0; buffer < BEGONIA_DISPLAY_BUFFERS; buffer++) {
        volatile uint32_t *page = pixels + buffer * BEGONIA_DISPLAY_PIXELS;
        for (unsigned i = 0; i < BEGONIA_DISPLAY_PIXELS; i++) page[i] = 0xff000000U;
        for (unsigned y = 0; y < 24; y++) {
            for (unsigned x = 0; x < BEGONIA_DISPLAY_WIDTH; x++) {
                page[y * BEGONIA_DISPLAY_WIDTH + x] = x < 267 ? 0xffff0000U :
                    x < 534 ? 0xff00ff00U : 0xff0000ffU;
            }
        }
    }
}
#endif
