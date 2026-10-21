/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 A04BR3 contributors.
 * CPU-only fragment USC launch producer; not a pixel-event/EOT program.
 * Mesa pvr_pds_kick_usc and original board FUN_0013cd1c agree on data8/code1
 * DWORDs. Original FUN_0013c5f8 supplies task control. See .sources.json.
 * Does NOT load shared constants, map/cache memory, build a transfer task,
 * replace EOT or prove BVNC shader compatibility / firmware acceptance.
 */
#ifndef A04BR3_ROGUE_FRAGMENT_KICK_H
#define A04BR3_ROGUE_FRAGMENT_KICK_H
#include <stdint.h>
#include <stddef.h>
enum { A04_RGX_FRAGMENT_DATA_WORDS=8, A04_RGX_FRAGMENT_CODE_WORDS=1,
       A04_RGX_FRAGMENT_CODE_OFFSET=32, A04_RGX_FRAGMENT_USED_BYTES=36,
       A04_RGX_FRAGMENT_IMAGE_BYTES=64 };
struct a04_rgx_fragment_kick { uint8_t image[A04_RGX_FRAGMENT_IMAGE_BYTES]; };
/* Deliberately narrow native core call: constant0, cc=0, dual-phase=0,
 * instance rate=0 or sample rate=2. The caller supplies heap-relative USC
 * code offset and actual extent, not a host/physical address. Extent checking
 * prevents 32-bit executable-address wrap; it cannot prove mapping/ownership
 * or that arbitrary input code implements a valid fragment shader.
 * Return nonzero leaves all output bytes unchanged. */
static inline int a04_rgx_fragment_kick_build(struct a04_rgx_fragment_kick *out,
    size_t out_bytes,uint64_t usc_heap_offset,size_t usc_code_bytes,
    unsigned usc_temps,unsigned sample_rate)
{
    if(!out || out_bytes<sizeof(*out) ||
       (uintptr_t)out>UINTPTR_MAX-sizeof(*out) ||
       !usc_code_bytes || (usc_code_bytes&3) || (usc_heap_offset&3) ||
       usc_heap_offset>UINT64_C(0xfffffffc) ||
       usc_code_bytes>UINT64_C(0x100000000)-usc_heap_offset ||
       usc_temps>252 || (sample_rate!=0 && sample_rate!=2))return 1;
    struct a04_rgx_fragment_kick made={{0}};
    uint64_t task=usc_heap_offset | ((uint64_t)((usc_temps+3)/4)<<35) |
                  ((uint64_t)sample_rate<<33);
    for(unsigned i=0;i<8;i++)made.image[i]=(uint8_t)(task>>(i*8));
    /* DOUTU const64[0], END; identical to native flags=0, offset=0. */
    const uint32_t doutu=(15u<<28)|(1u<<26)|2u;
    for(unsigned i=0;i<4;i++)made.image[32+i]=(uint8_t)(doutu>>(i*8));
    *out=made;return 0;
}
#endif
