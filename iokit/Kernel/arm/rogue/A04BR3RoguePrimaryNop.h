/*
 * Copyright © 2022, 2024 Imagination Technologies Ltd.
 * Copyright (c) 2026 A04BR3 contributors.
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
 * Source-built dedicated primary NOP+END, NOT a pixel-writing shader or EOT.
 * Pinned PCO: pco_map.py O_NOP control header; pco_end.c appends NOP.end;
 * pco_group_instrs.c DA0 for no refs, icache8 final alignment;
 * pco_binary.c pco_encode_align. Named forward assembly, no binary decoding.
 * Independently compared with original PCO encoders and external REA bytes
 * at original libsrv_um.so150604, consumed through context5490/metadata5498.
 * It is NOT blob-table index1345 and requires no USC temps/shared registers.
 */
#ifndef A04BR3_ROGUE_PRIMARY_NOP_H
#define A04BR3_ROGUE_PRIMARY_NOP_H
#include <stdint.h>
#include <stddef.h>
enum { A04_RGX_PRIMARY_NOP_BYTES=8,A04_RGX_PRIMARY_NOP_TEMPS=0 };
static inline int a04_rgx_primary_nop_generate(void *out,size_t capacity)
{
    if(!out || capacity<8 || (uintptr_t)out>UINTPTR_MAX-8)return 1;
    uint8_t *p=(uint8_t *)out;
    const unsigned length=4,da=0,ext=1,control_type=3,nop_op=7,end=1;
    p[0]=(uint8_t)((da<<4)|length);
    p[1]=(uint8_t)(ext<<7); /* no refs, OLCHK/W0/W1/CC all0 */
    p[2]=(uint8_t)((end<<7)|(control_type<<5)|(nop_op<<1));
    p[3]=0; /* I_NOP reserved byte */
    p[4]=(uint8_t)(0xf0u|2u); /*4 bytes final alignment */
    for(unsigned i=5;i<8;i++)p[i]=0xff;
    return 0;
}
#endif
