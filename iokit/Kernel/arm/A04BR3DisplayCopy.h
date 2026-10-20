/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Optional cached comparison: skip expensive unchanged Device-memory stores.
 * Shadow storage belongs to the compositor, never to a user mapping.
 */
#ifndef A04BR3_DISPLAY_COPY_H
#define A04BR3_DISPLAY_COPY_H
static inline unsigned
a04br3_copy_changed_pixels(volatile uint32_t *out, uint32_t *shadow,
    const uint32_t *in, unsigned count)
{
    unsigned written = 0;
    for (unsigned x = 0; x < count; x++) {
        uint32_t pixel = in[x] | 0xff000000U;
        if (shadow[x] != pixel) {
            out[x] = pixel;
            shadow[x] = pixel;
            written++;
        }
    }
    return written;
}
#endif
