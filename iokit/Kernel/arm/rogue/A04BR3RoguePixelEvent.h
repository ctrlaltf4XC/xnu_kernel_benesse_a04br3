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
 * Source-reconstructed single-RT PDS pixel-event producer, NOT a USC assembler.
 * Mesa e1ef8f046c6e37ec1e2a13229560fbb824e5118f pvr_pds.c pixel-event,
 * pvr_rogue_pds_defs.h DOUT fields; corroborated against original board
 * m1.11 libsrv_um.so FUN_0013c634 and task-control FUN_0013c5f8.
 * Original DDK advertises 2*n+3 code words (5 here), although n+1 are written.
 * Preserve that reservation and zero its padding. Mesa advertises only n+1.
 * This produces CPU bytes only: caller must provide a proven USC EOT program,
 * heap-relative executable address and actual PBE state, then own/cache/map
 * the allocation. No firmware submission, clear, or completion is implied.
 */
#ifndef A04BR3_ROGUE_PIXEL_EVENT_H
#define A04BR3_ROGUE_PIXEL_EVENT_H
#include <stdint.h>
enum { A04_RGX_PIXEL_EVENT_IMAGE_BYTES=64,
       A04_RGX_PIXEL_EVENT_DATA_WORDS=5,
       A04_RGX_PIXEL_EVENT_CODE_WORDS=5,
       A04_RGX_PIXEL_EVENT_CODE_WRITTEN_WORDS=2,
       A04_RGX_PIXEL_EVENT_CODE_OFFSET=32,
       A04_RGX_PIXEL_EVENT_USED_BYTES=52 };
struct a04_rgx_pixel_event {
    uint8_t image[A04_RGX_PIXEL_EVENT_IMAGE_BYTES];
};
static inline void a04_rgx_pixel_event_put32(uint8_t *out,uint32_t v)
{
    for(unsigned i=0;i<4;i++)out[i]=(uint8_t)(v>>(8*i));
}
/* Minimal source DOUT encoding: cc=0; DOUTW reads common-store control const4
 * and 64-bit state const1. DOUTU ends PDS and starts USC from 64-bit const0.
 * Fixed indices come from allocating 2 task words, 2 state words, 1 control.
 */
static inline int a04_rgx_pixel_event_build(struct a04_rgx_pixel_event *out,
    uint64_t usc_heap_offset,unsigned usc_temps,uint32_t pbe0,uint32_t pbe1)
{
    /* Native masks address alignment and temp overflow. Reject instead of
     * silently launching another address or underallocating USC registers. */
    if(!out || usc_heap_offset>UINT64_C(0xfffffffc) ||
       (usc_heap_offset&3) || usc_temps>252)return 1;
    uint64_t task=usc_heap_offset|((uint64_t)((usc_temps+3)/4)<<35);
    struct a04_rgx_pixel_event generated={};
    a04_rgx_pixel_event_put32(generated.image,(uint32_t)task);
    a04_rgx_pixel_event_put32(generated.image+4,(uint32_t)(task>>32));
    a04_rgx_pixel_event_put32(generated.image+8,pbe0);
    a04_rgx_pixel_event_put32(generated.image+12,pbe1);
    /* last=1, common-store=1, lower64=2, shared-register destination0;
     * board-native source has no DOUTW SLC cache-control bit. */
    a04_rgx_pixel_event_put32(generated.image+16,(1u<<31)|(1u<<28)|2u);
    uint32_t doutw=(15u<<28)|(4u<<16)|(1u<<8)|1u;
    uint32_t doutu=(15u<<28)|(1u<<26)|2u;
    a04_rgx_pixel_event_put32(generated.image+32,doutw);
    a04_rgx_pixel_event_put32(generated.image+36,doutu);
    *out=generated;return 0;
}
#endif
