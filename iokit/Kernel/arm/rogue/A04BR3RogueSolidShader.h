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
 * Bounded source-built USC fragment program: copy1..4 raw shared constants
 * into pixel output0..3, then END. No texture/AGX/Metal/float conversion.
 * Forward lowering of pinned Mesa e1ef8f046c6e37ec1e2a13229560fbb824e5118f:
 * pvr_usc.c pvr_uscgen_clear_attach register path; pco_trans_nir frag_store;
 * pco_legalize.c post-RA MOV->MBYP for SHARED; pco_map.py O_MBYP and
 * PIXOUT->SPECIAL mapping; pco_map.h.py register indices;
 * pco_isa.py named encodings; pco_group_instrs.c/pco_binary.c final alignment.
 * This is a narrow independently built program, not a full NIR compiler and
 * not a claim of BVNC22.40 compatibility. Target dispatch/PDS/uniform upload,
 * output packing/PBE and physical pixels need separate validated integration.
 * It is NOT the existing EOT program and cannot replace EOT/pixel-event data.
 * No runtime driver/registry/capability/cache/submission state is modified.
 */
#ifndef A04BR3_ROGUE_SOLID_SHADER_H
#define A04BR3_ROGUE_SOLID_SHADER_H
#include <stdint.h>
#include <stddef.h>
enum { A04_RGX_SOLID_MAX_CODE=48, A04_RGX_SOLID_MAX_WORDS=4,
       A04_RGX_SOLID_SHARED_LIMIT=256, A04_RGX_SOLID_ICACHE_ALIGN=8 };
struct a04_rgx_solid_shader {
    uint8_t code[A04_RGX_SOLID_MAX_CODE];
    uint8_t shared_data[4*A04_RGX_SOLID_MAX_WORDS];
    uint32_t code_bytes,shared_first,shared_words,pixel_first,pixel_words,temps;
};
static inline int a04_rgx_solid_span(const void *p,size_t bytes)
{return p && bytes && (uintptr_t)p<=UINTPTR_MAX-bytes;}
/* Emit one O_MBYP group using the source-selected encodings: MAIN3 + SNGL1
 * + LOWER_1lo_3b11i_2m3 (SHARED bank4) + unused UPPER1 + ISS1 + DEST1.
 * Phase0 DA=header3; destination SPECIAL32..35 is PIXOUT0..3, not memory.
 * Internal FT0 is forwarded to w0 via IS4=FT0. No allocated USC temp needed.
 */
static inline int a04_rgx_solid_emit(uint8_t *out,size_t capacity,unsigned shared,unsigned pixel,
    unsigned last,unsigned group_bytes)
{
    if(shared>=A04_RGX_SOLID_SHARED_LIMIT || pixel>3 || last>1 ||
       group_bytes<10 || group_bytes>16 || (group_bytes&1) || capacity<group_bytes ||
       !a04_rgx_solid_span(out,group_bytes))return 1;
    const unsigned header=3,words=group_bytes/2,oporg_p0=0;
    const unsigned olchk=1,w0p=1,special=0,shared_bank=4;
    out[0]=(uint8_t)((header<<4)|(words&15));
    out[1]=(uint8_t)((1u<<7)|(oporg_p0<<4)|(olchk<<3)|(w0p<<1));
    out[2]=(uint8_t)(last<<7); // ALUTYPE MAIN0, execution condition E1_ZX0, repeat1
    out[3]=(uint8_t)((4u<<5)|7u); // MAIN_SNGL + SNGL_BYP, no neg/abs
    out[4]=(uint8_t)((1u<<7)|((shared_bank&1u)<<6)|(shared&63u));
    out[5]=(uint8_t)(((shared_bank>>1)<<2)|((shared>>6)&3u));
    out[6]=(uint8_t)((shared>>8)&7u); // remaining source bits/reserved0
    out[7]=0; // unused upper source SPECIAL0, non-control MAIN still encodes it
    out[8]=0; // ISS IS4=FT0, unused selectors0
    out[9]=(uint8_t)((special<<6)|(32u+pixel));
    if(group_bytes>10){
        // pco_encode_align encodes even alignment bytes with f0|wordcount.
        unsigned padding=group_bytes-10;
        out[10]=(uint8_t)(0xf0u|padding/2);
        for(unsigned i=11;i<group_bytes;i++)out[i]=0xff;
    }
    return 0;
}
/* Success means GENERATED CPU PROGRAM ONLY. Raw constant bytes are copied in
 * little-endian lane order, without interpreting them as a surface format.
 * shared_capacity is the future owner's actual shared allocation bound;
 * <=256 is a deliberately restricted local policy, not a hardware feature.
 * On every rejection output and input are unchanged. No user shader words,
 * physical pointers, indirect register indices or arbitrary opcode accepted.
 */
static inline int a04_rgx_solid_shader_compile(struct a04_rgx_solid_shader *out,
    size_t out_bytes,const void *constants,size_t constant_bytes,unsigned shared_first,
    unsigned shared_capacity,unsigned count,unsigned pixel_first)
{
    if(!count || count>4 || pixel_first>3 || count>4-pixel_first ||
       !shared_capacity || shared_capacity>A04_RGX_SOLID_SHARED_LIMIT ||
       shared_first>=shared_capacity || count>shared_capacity-shared_first ||
       constant_bytes!=(size_t)count*4 || out_bytes<sizeof(*out) ||
       !a04_rgx_solid_span(out,sizeof(*out)) || ((uintptr_t)out&3) ||
       !a04_rgx_solid_span(constants,constant_bytes))return 1;
    if((uintptr_t)out<(uintptr_t)constants+constant_bytes &&
       (uintptr_t)constants<(uintptr_t)out+sizeof(*out))return 1;
    struct a04_rgx_solid_shader generated;
    for(size_t i=0;i<sizeof(generated);i++)((uint8_t *)&generated)[i]=0;
    unsigned sizes[4]={10,10,10,10};
    // Exact PCO grouping policy: align the last group's size AND its offset
    // to ROGUE_ICACHE_ALIGN8, growing the penultimate group as necessary.
    sizes[count-1]=16;
    unsigned last_offset=(count-1)*10;
    if(count>1 && last_offset%8)sizes[count-2]+=8-last_offset%8;
    unsigned at=0;
    for(unsigned i=0;i<count;i++){
        if(a04_rgx_solid_emit(generated.code+at,sizeof(generated.code)-at,
            shared_first+i,pixel_first+i,i==count-1,sizes[i]))return 1;
        at+=sizes[i];
    }
    const uint8_t *p=(const uint8_t *)constants;
    for(unsigned i=0;i<constant_bytes;i++)generated.shared_data[i]=p[i];
    generated.code_bytes=at;generated.shared_first=shared_first;
    generated.shared_words=count;generated.pixel_first=pixel_first;
    generated.pixel_words=count;generated.temps=0;
    *out=generated;return 0;
}
#endif
