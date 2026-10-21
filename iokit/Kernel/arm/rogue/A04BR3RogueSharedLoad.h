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
 * Narrow original m1.11 clear-secondary program, independently generated.
 * FUN13ca3c: clear=1, packed=0, even destination, kick_usc=0 -> data8 words.
 * FUN13c794: same configuration -> two IF0 DOUTWs + HALT, code3 words.
 * Mesa pvr_pds_pixel_shader_uniform_texture_data / pixel_shader_sa_code agree.
 * This is NOT EOT or a replacement fragment launch. Shared register contents
 * exist per USC task/common-store allocation; never a CPU-addressable buffer.
 */
#ifndef A04BR3_ROGUE_SHARED_LOAD_H
#define A04BR3_ROGUE_SHARED_LOAD_H
#include <stdint.h>
#include <stddef.h>
enum { A04_RGX_SHARED_LOAD_IMAGE_BYTES=64, A04_RGX_SHARED_LOAD_DATA_WORDS=8,
       A04_RGX_SHARED_LOAD_CODE_OFFSET=32, A04_RGX_SHARED_LOAD_CODE_WORDS=3,
       A04_RGX_SHARED_LOAD_TEMPS=1 };
struct a04_rgx_shared_load { uint8_t image[A04_RGX_SHARED_LOAD_IMAGE_BYTES]; };
static inline int a04_rgx_shared_load_build(struct a04_rgx_shared_load *out,
    size_t out_bytes,const void *constants,size_t bytes,unsigned first,unsigned capacity)
{
    uintptr_t o=(uintptr_t)out,c=(uintptr_t)constants;
    if(!out || !constants || out_bytes<sizeof(*out) || bytes!=16 ||
       o>UINTPTR_MAX-sizeof(*out) || c>UINTPTR_MAX-16 ||
       (o<c+16 && c<o+sizeof(*out)) ||
       !capacity || capacity>256 || (first&1) || first>=capacity || capacity-first<4)return 1;
    struct a04_rgx_shared_load made={{0}};
    for(unsigned i=0;i<16;i++)made.image[i]=((const uint8_t *)constants)[i];
    const uint32_t controls[2]={(first<<13)|0x10000002u,
                               ((first+2)<<13)|0x90000002u};
    for(unsigned n=0;n<2;n++)for(unsigned i=0;i<4;i++)
        made.image[16+n*8+i]=(uint8_t)(controls[n]>>(8*i));
    /* Exact recovered target IF0 predicate; final DOUTW ends, explicit HALT
     * is still part of the original reserved/emitted code. No guessed task. */
    const uint32_t code[3]={(15u<<28)|(1u<<27)|(4u<<16)|1u,
        (15u<<28)|(1u<<27)|(1u<<26)|(6u<<16)|(1u<<8)|1u,
        (13u<<28)|(3u<<24)};
    for(unsigned n=0;n<3;n++)for(unsigned i=0;i<4;i++)
        made.image[32+n*4+i]=(uint8_t)(code[n]>>(8*i));
    *out=made;return 0;
}
#endif
