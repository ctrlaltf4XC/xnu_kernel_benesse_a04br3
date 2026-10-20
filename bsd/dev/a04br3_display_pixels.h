/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pixel contract follows mouseos/edk2-mtk SimpleFbDxe at 1982d888.
 * Call only with all four reserved framebuffer surfaces mapped writable.
 */
#ifndef A04BR3_DISPLAY_PIXELS_H
#define A04BR3_DISPLAY_PIXELS_H
#define A04BR3_DISPLAY_WIDTH 800U
#define A04BR3_DISPLAY_HEIGHT 1280U
#define A04BR3_DISPLAY_BUFFERS 4U
#define A04BR3_DISPLAY_PIXELS (A04BR3_DISPLAY_WIDTH * A04BR3_DISPLAY_HEIGHT)
#define A04BR3_DISPLAY_BYTES (A04BR3_DISPLAY_PIXELS * 4UL * A04BR3_DISPLAY_BUFFERS)
static inline void a04br3_display_paint(volatile uint32_t *pixels)
{
    for (unsigned buffer = 0; buffer < A04BR3_DISPLAY_BUFFERS; buffer++) {
        volatile uint32_t *page = pixels + buffer * A04BR3_DISPLAY_PIXELS;
        for (unsigned i = 0; i < A04BR3_DISPLAY_PIXELS; i++) page[i] = 0xff000000U;
        for (unsigned y = 0; y < 24; y++) {
            for (unsigned x = 0; x < A04BR3_DISPLAY_WIDTH; x++) {
                page[y * A04BR3_DISPLAY_WIDTH + x] = x < 267 ? 0xffff0000U :
                    x < 534 ? 0xff00ff00U : 0xff0000ffU;
            }
        }
    }
}
#endif
