/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026 A04BR3 contributors.
 * Original-board canonical32x32 native22 background STATE producer.
 * See .sources.json for exact native functions/table evidence and limitations.
 * Generates real shared/sampler/image + matching PDS DMA bytes, not an opaque
 * metadata decoder. No fragment USC blob/DOUTU/full208/queue is accepted.
 */
#ifndef A04BR3_ROGUE_NATIVE22_STATE_H
#define A04BR3_ROGUE_NATIVE22_STATE_H
#include "A04BR3RogueISPFill.h"
enum {A04_RGX_NATIVE22_SHARED_WORDS=9,A04_RGX_NATIVE22_USC_TEMPS=4,
      A04_RGX_NATIVE22_REQUIRED_USC_BYTES=152,A04_RGX_NATIVE22_PDS_TEMPS=1,
      A04_RGX_NATIVE22_PDS_DATA_WORDS=3,A04_RGX_NATIVE22_PDS_CODE_WORDS=3,
      A04_RGX_NATIVE22_FIRST_PADDED_WORDS=0,A04_RGX_NATIVE22_SECOND_PADDED_WORDS=4};
/* Profile is FIXED: native22, linear32x32/pitch32, point0, one sample,
 * destination==source, no flags/scaling/subrect/MSAA/planes. Source key8/row0
 * is valid only for this internally constructed property tuple. */
struct a04_rgx_native22_state_input {
    a04_rgx_bif heaps[4];
    a04_rgx_isp_span target,shared,pds_data,pds_code;
};
struct a04_rgx_native22_state_image {
    uint8_t shared[36],pds_data[16],pds_code[16];
    a04_rgx_isp_span shared_destination,data_destination,code_destination,target_reference;
};
static inline int a04_rgx_native22_state_build(a04_rgx_native22_state_image *out,
    const a04_rgx_native22_state_input *input)
{
    if(!a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
       !a04_rgx_fw_pointer(input,sizeof(*input)) || ((uintptr_t)input&7) ||
       a04_rgx_fw_alias(out,sizeof(*out),input,sizeof(*input)))return 1;
    const auto in=*input;
    if(in.heaps[0].va!=UINT64_C(0x65c0000000) || in.heaps[1].va!=UINT64_C(0xa800000000) ||
       in.heaps[2].va!=UINT64_C(0xba00000000) || in.heaps[3].va>=A04_RGX_ISP_HEAP_BYTES ||
       in.heaps[3].map_bytes>A04_RGX_ISP_HEAP_BYTES-in.heaps[3].va ||
       in.heaps[0].read_only || in.heaps[1].read_only || !in.heaps[2].read_only ||
       in.heaps[3].read_only || a04_rgx_heap_map_validate(in.heaps,4))return 1;
    for(const auto &h:in.heaps) {
        const a04_rgx_render_buffer parts[]={h.pc,h.pd,h.pt,h.data};
        for(const auto &b:parts)if(a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes) ||
            a04_rgx_fw_alias(input,sizeof(*input),b.cpu,b.bytes))return 1;
        for(unsigned n=0;n<h.map_bytes;n+=4096) {
            uint64_t pa=0;if(a04_rgx_bif_translate(&h,h.va+n,&pa) || pa!=h.data.pa+n)return 1;
        }
    }
    if(in.target.bytes!=4096 || in.shared.bytes!=36 || in.pds_data.bytes!=16 || in.pds_code.bytes!=16 ||
       !a04_rgx_isp_span_in(in.heaps[0],in.target,128) ||
       !a04_rgx_isp_span_in(in.heaps[0],in.shared,16) ||
       !a04_rgx_isp_span_in(in.heaps[1],in.pds_data,16) ||
       !a04_rgx_isp_span_in(in.heaps[1],in.pds_code,16) ||
       a04_rgx_fw_overlap(in.target.va,4096,in.shared.va,36) ||
       a04_rgx_fw_overlap(in.pds_data.va,16,in.pds_code.va,16))return 1;
    a04_rgx_native22_state_image made; a04_rgx_fw_zero(&made,sizeof(made));
    /* FUN12d0bc filter0/4, layout0, native22 single plane. All indexed
     * metadata offsets below belong to canonical key8/row0, NOT a generic
     * shader layout: sampler DWORD0, texture DWORD4, literal DWORD8. */
    a04_rgx_isp_put64(made.shared,UINT64_C(0x240000000000));
    /* FUN13eeac native22 row26: format12, swizzle0/1/2/3, linear2D4;
     * width/height minus1; samplecount1. No modern format enum substituted. */
    const uint64_t texture0=(UINT64_C(12)<<27)|(UINT64_C(1)<<11)|
        (UINT64_C(2)<<8)|(UINT64_C(3)<<5)|4|
        (UINT64_C(31)<<34)|(UINT64_C(31)<<48);
    /* Surface setup12d758/12d28c: stride32, one level, plane1, flags0;
     * format22 metadata channelFlags0082 has bit2 clear -> native bit59. */
    const uint64_t texture1=(in.target.va<<14)|31|(UINT64_C(1)<<60)|(UINT64_C(1)<<59);
    a04_rgx_isp_put64(made.shared+16,texture0);a04_rgx_isp_put64(made.shared+24,texture1);
    /* row0 literal-count1, offset[0]=8, value[0]=255; FUN129f88 copies it. */
    a04_rgx_isp_put32(made.shared+32,255);
    /* One DMA into common-store0,9 DWORDs. Native13ca3c data3 padded4,
     * native13c794 default one-kick code: IF0 branch+2, DOUTD.END, HALT.
     * This is not the IF0 DOUTW immediate-colour source program. */
    a04_rgx_isp_put64(made.pds_data,in.shared.va);
    a04_rgx_isp_put32(made.pds_data+8,0xf0000000U|9U);
    a04_rgx_isp_put32(made.pds_code,(12U<<28)|(1U<<24)|(1U<<22)|2U);
    a04_rgx_isp_put32(made.pds_code+4,(15U<<28)|(1U<<26)|(2U<<16));
    a04_rgx_isp_put32(made.pds_code+8,(13U<<28)|(3U<<24));
    made.shared_destination=in.shared;made.data_destination=in.pds_data;
    made.code_destination=in.pds_code;made.target_reference=in.target;
    *out=made;return 0;
}
#endif
