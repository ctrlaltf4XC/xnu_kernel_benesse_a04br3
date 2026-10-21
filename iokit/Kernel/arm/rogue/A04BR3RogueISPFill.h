/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source reconstruction of original-board FUN12934c + FUN12dc50, source0,
 * one canonical32x32 rectangle. See A04BR3RogueISPFill.sources.json.
 * Generates control128 + quad88 CPU bytes only. Does not build a complete208
 * task, certify shader contents, publish memory, submit, or claim pixels.
 * Native12934c additionally needs a real destination-as-source background
 * producer. Existing zero-source live TransferFill is NOT changed here.
 */
#ifndef A04BR3_ROGUE_ISP_FILL_H
#define A04BR3_ROGUE_ISP_FILL_H
#include "A04BR3RogueHeapMap.h"
enum { A04_RGX_ISP_CONTROL_BYTES=128,A04_RGX_ISP_QUAD_BYTES=88 };
/* MT8167 user config -> core22.40.54.30 -> BRN65273 -> rgxinit heap choice.
 * Zero is a real ISP heap base, not an absent-address sentinel.
 */
static const uint64_t A04_RGX_ISP_HEAP_BASE=UINT64_C(0);
static const uint64_t A04_RGX_ISP_HEAP_BYTES=UINT64_C(0x40000000);
struct a04_rgx_isp_span {uint64_t va,bytes;};
struct a04_rgx_isp_fill_input {
    a04_rgx_bif heaps[4]; /* general, PDS, USC, ISP; shared cold catalogue */
    a04_rgx_isp_span target,fragment_pds,fragment_usc,control,quad;
    uint32_t native_format,width,height,pitch_pixels,source_count;
    int32_t x0,y0,x1,y1;
    uint32_t pds_temps; /* native P+1a4 may carry from the background producer */
};
struct a04_rgx_isp_fill_image {
    uint8_t control[A04_RGX_ISP_CONTROL_BYTES];
    uint8_t quad[A04_RGX_ISP_QUAD_BYTES];
    a04_rgx_isp_span control_destination,quad_destination;
    a04_rgx_isp_span target_reference,fragment_pds_reference,fragment_usc_reference;
    uint64_t raw60,raw68,raw70,raw78; /* partial old208 fields ONLY */
};
static inline void a04_rgx_isp_put32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline void a04_rgx_isp_put64(uint8_t *p,uint64_t v)
{for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline bool a04_rgx_isp_span_in(const a04_rgx_bif &h,
    const a04_rgx_isp_span &s,uint64_t alignment)
{
    return s.bytes && !(s.va&(alignment-1)) && s.va>=h.va &&
        s.va-h.va<h.map_bytes && s.bytes<=h.map_bytes-(s.va-h.va);
}
/* Pure preflight and local construction; failure preserves every output and
 * backing-buffer byte. All four mappings must actually walk to their owned
 * physical data. This is a range/mapping contract, not a lifetime retain or
 * proof that an input program is compatible with this GPU.
 * Primary secondary/varying data counts/addresses are the native empty case;
 * pds_temps is preserved explicitly, never guessed from absence of data.
 */
static inline int a04_rgx_isp_fill_build(a04_rgx_isp_fill_image *out,
    const a04_rgx_isp_fill_input *input)
{
    if(!a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
       !a04_rgx_fw_pointer(input,sizeof(*input)) || ((uintptr_t)input&7) ||
       a04_rgx_fw_alias(out,sizeof(*out),input,sizeof(*input)))return 1;
    const a04_rgx_isp_fill_input in=*input;
    if(in.native_format!=0x22 || in.width!=32 || in.height!=32 || in.pitch_pixels!=32 || in.source_count ||
       in.x0!=0 || in.y0!=0 || in.x1!=32 || in.y1!=32 || in.pds_temps>60 ||
       in.heaps[0].va!=UINT64_C(0x65c0000000) ||
       in.heaps[1].va!=UINT64_C(0xa800000000) ||
       in.heaps[2].va!=UINT64_C(0xba00000000) ||
       in.heaps[3].va>=A04_RGX_ISP_HEAP_BYTES ||
       in.heaps[3].map_bytes>A04_RGX_ISP_HEAP_BYTES-in.heaps[3].va ||
       in.heaps[0].read_only || in.heaps[1].read_only ||
       !in.heaps[2].read_only || in.heaps[3].read_only ||
       a04_rgx_heap_map_validate(in.heaps,4))return 1;
    for(const auto &h:in.heaps) {
        const a04_rgx_render_buffer buffers[]={h.pc,h.pd,h.pt,h.data};
        for(const auto &b:buffers)
            if(a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes) ||
               a04_rgx_fw_alias(input,sizeof(*input),b.cpu,b.bytes))return 1;
        for(unsigned n=0;n<h.map_bytes;n+=4096) {
            uint64_t pa=0;
            if(a04_rgx_bif_translate(&h,h.va+n,&pa) || pa!=h.data.pa+n)return 1;
        }
    }
    if(in.target.bytes!=4096 || in.fragment_pds.bytes<36 ||
       (in.fragment_pds.bytes&3) || in.fragment_usc.bytes<8 ||
       (in.fragment_usc.bytes&3) || in.control.bytes!=128 || in.quad.bytes!=88 ||
       !a04_rgx_isp_span_in(in.heaps[0],in.target,128) ||
       !a04_rgx_isp_span_in(in.heaps[1],in.fragment_pds,16) ||
       !a04_rgx_isp_span_in(in.heaps[2],in.fragment_usc,4) ||
       !a04_rgx_isp_span_in(in.heaps[3],in.control,8) ||
       !a04_rgx_isp_span_in(in.heaps[3],in.quad,8) ||
       a04_rgx_fw_overlap(in.control.va,in.control.bytes,in.quad.va,in.quad.bytes))return 1;
    const uint64_t pds=in.fragment_pds.va-in.heaps[1].va;
    const uint64_t control=in.control.va-A04_RGX_ISP_HEAP_BASE;
    const uint64_t quad=in.quad.va-A04_RGX_ISP_HEAP_BASE;
    if(pds>UINT64_C(0xfffffff0) || control>UINT64_C(0xfffffffc) ||
       quad>UINT64_C(0xfffffff8))return 1;
    a04_rgx_isp_fill_image made={};
    made.control_destination=in.control;made.quad_destination=in.quad;
    made.target_reference=in.target;made.fragment_pds_reference=in.fragment_pds;
    made.fragment_usc_reference=in.fragment_usc;
    /* Native one fill entry plus explicit end bit. Remaining control storage
     * is canonical zero padding, not additional executable ISP commands. */
    a04_rgx_isp_put64(made.control,
        ((quad<<30)&UINT64_C(0xfffffffe00000000))|UINT64_C(0x10000032a));
    a04_rgx_isp_put32(made.quad,0x60); /* onequad, no source UV data */
    a04_rgx_isp_put32(made.quad+8,0x2e0000);
    /* FUN12dc50 one-quad index expression, not USC/AGX instructions. */
    const uint32_t a=0xc0,b=0x1400;
    const uint64_t indices=((a-0xa0)&0x120)|((b-0xc00)&0x2800)|
        (uint64_t)((((a-0x80)&0x140)|((b-0x1000)&0x2400))<<16)|0x30000U|
        ((uint64_t)((a&0x1c0)|(b&0x3400)|7)<<48)|
        ((uint64_t)(((a-0x20)&0x1a0)|((b+0x400)&0x3800)|4)<<32);
    a04_rgx_isp_put64(made.quad+16,indices);
    const int32_t x[]={in.x0,in.x1,in.x0,in.x1};
    const int32_t y[]={in.y0,in.y0,in.y1,in.y1};
    for(unsigned n=0;n<4;n++) {
        const uint64_t vx=(uint32_t)(x[n]+2048)*16U;
        const uint64_t vy=(uint32_t)(y[n]+2048)*16U;
        a04_rgx_isp_put64(made.quad+24+8*n,vx|(vy<<17));
    }
    a04_rgx_isp_put32(made.quad+56,(uint32_t)pds);
    /* Empty secondary/varyings: all other pointer/count words stay zero. */
    a04_rgx_isp_put32(made.quad+64,(in.pds_temps+3)/4);
    a04_rgx_isp_put32(made.quad+80,0x4000); /* primary shared count0 */
    made.raw60=2;made.raw68=control;
    made.raw70=A04_RGX_ISP_HEAP_BASE&UINT64_C(0xfc00000000);made.raw78=0;
    *out=made;return 0;
}
#endif
