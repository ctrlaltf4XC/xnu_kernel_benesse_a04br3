/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026 A04BR3 contributors.
 * Cold fourheap resource preparation only. No complete208 or submission API.
 * Owns14 allocation leases including ISP PD/PT/data, partitions sampler state
 * from pixel/guard memory, and binds source-generated primaryNOP to ISP quad.
 * Native background152-byte USC remains absent; these images cannot be used
 * as a complete rendering task. Existing default clear/EOT/runtime unchanged.
 */
#ifndef A04BR3_ROGUE_NATIVE_RESOURCES_H
#define A04BR3_ROGUE_NATIVE_RESOURCES_H
#include "A04BR3RogueTransferFill.h"
#include "A04BR3RoguePrimaryNop.h"
#include "A04BR3RogueFragmentKick.h"
#include "A04BR3RogueNative22State.h"
enum { A04_RGX_NATIVE_PDS_BYTES=288,A04_RGX_NATIVE_USC_BYTES=104,
       A04_RGX_NATIVE_SHARED_OFFSET=8192 };
struct a04_rgx_native_resource_image {
    uint8_t pds[A04_RGX_NATIVE_PDS_BYTES],usc[A04_RGX_NATIVE_USC_BYTES];
    a04_rgx_native22_state_image background;
    a04_rgx_isp_fill_image isp;
};
static inline bool a04_rgx_native_same_buffer(const a04_rgx_render_buffer &a,
    const a04_rgx_render_buffer &b)
{return a.cpu==b.cpu && a.pa==b.pa && a.bytes==b.bytes;}
static inline bool a04_rgx_native_same_bif(const a04_rgx_bif &a,const a04_rgx_bif &b)
{return a.va==b.va && a.map_bytes==b.map_bytes && a.read_only==b.read_only &&
    a04_rgx_native_same_buffer(a.pc,b.pc) && a04_rgx_native_same_buffer(a.pd,b.pd) &&
    a04_rgx_native_same_buffer(a.pt,b.pt) && a04_rgx_native_same_buffer(a.data,b.data);}
static inline int a04_rgx_native_resource_build(a04_rgx_native_resource_image *out,size_t out_bytes,
    const a04_rgx_render_resources *r,size_t r_bytes,const a04_rgx_bif *heaps,size_t heap_bytes)
{
    if(out_bytes<sizeof(*out) || r_bytes<sizeof(*r) || heap_bytes!=4*sizeof(*heaps) ||
       !a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
       !a04_rgx_fw_pointer(r,sizeof(*r)) || ((uintptr_t)r&7) ||
       !a04_rgx_fw_pointer(heaps,heap_bytes) || ((uintptr_t)heaps&7) ||
       a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)) ||
       a04_rgx_fw_alias(out,sizeof(*out),heaps,heap_bytes))return 1;
    if(!a04_rgx_render_view_valid(r) || !a04_rgx_native_same_bif(r->bif,heaps[0]) ||
       a04_rgx_heap_map_validate(heaps,4))return 1;
    for(unsigned n=0;n<4;n++) {
        if(heaps[n].map_bytes!=65536 || heaps[n].data.bytes!=65536)return 1;
        const a04_rgx_render_buffer parts[]={heaps[n].pc,heaps[n].pd,heaps[n].pt,heaps[n].data};
        for(const auto &b:parts) {
            /* The bank is outside all four mappings, including the ISP heap
             * not covered by the old threeheap clear builder. */
            if(a04_rgx_render_buffers_overlap(r->bank,b) ||
               a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes) ||
               a04_rgx_fw_alias(r,sizeof(*r),b.cpu,b.bytes) ||
               a04_rgx_fw_alias(heaps,heap_bytes,b.cpu,b.bytes))return 1;
        }
    }
    if(a04_rgx_fw_alias(out,sizeof(*out),r->bank.cpu,r->bank.bytes) ||
       a04_rgx_fw_alias(r,sizeof(*r),r->bank.cpu,r->bank.bytes) ||
       a04_rgx_fw_alias(heaps,heap_bytes,r->bank.cpu,r->bank.bytes))return 1;
    a04_rgx_native_resource_image made;a04_rgx_fw_zero(&made,sizeof(made));
    a04_rgx_tq_clear eot;
    if(a04_rgx_tq_clear_build(&eot,r,&heaps[1],&heaps[2]))return 1;
    for(unsigned n=0;n<64;n++)made.pds[n]=eot.pixel.image[n];
    for(unsigned n=0;n<12;n++)made.usc[n]=eot.usc[n];
    if(a04_rgx_primary_nop_generate(made.usc+96,8))return 1;
    a04_rgx_fragment_kick primary;
    if(a04_rgx_fragment_kick_build(&primary,sizeof(primary),96,8,0,0))return 1;
    for(unsigned n=0;n<64;n++)made.pds[192+n]=primary.image[n];
    a04_rgx_native22_state_input state={};
    for(unsigned n=0;n<4;n++)state.heaps[n]=heaps[n];
    state.target={heaps[0].va,4096};state.shared={heaps[0].va+8192,36};
    state.pds_data={heaps[1].va+256,16};state.pds_code={heaps[1].va+272,16};
    if(a04_rgx_native22_state_build(&made.background,&state))return 1;
    for(unsigned n=0;n<16;n++) {
        made.pds[256+n]=made.background.pds_data[n];made.pds[272+n]=made.background.pds_code[n];
    }
    a04_rgx_isp_fill_input isp={};for(unsigned n=0;n<4;n++)isp.heaps[n]=heaps[n];
    isp.target=state.target;isp.fragment_pds={heaps[1].va+192,64};isp.fragment_usc={heaps[2].va+96,8};
    isp.control={heaps[3].va,128};isp.quad={heaps[3].va+128,88};
    isp.native_format=0x22;isp.width=isp.height=isp.pitch_pixels=32;isp.x1=isp.y1=32;
    isp.pds_temps=A04_RGX_NATIVE22_PDS_TEMPS;
    if(a04_rgx_isp_fill_build(&made.isp,&isp))return 1;
    *out=made;return 0;
}
/* Callback0 means success. hold must retain the actual allocation covering
 * exact CPU/PA/size, drop balances it. clean includes store barrier; acquire
 * synchronizes CPU reads of the NC/WC mappings. No SLC/pixel proof implied.
 * Caller holds producer/power/lifecycle mutex and supplies a NEVER-REGISTERED
 * private cold bank/mappings. Zero CCB offsets alone cannot prove that fact. */
struct a04_rgx_native_resource_io {
    void *context;
    int (*hold)(void *,const a04_rgx_render_buffer *);
    void (*drop)(void *,const a04_rgx_render_buffer *);
    int (*clean)(void *,const a04_rgx_render_buffer *);
    int (*acquire)(void *);
};
class A04BR3RogueNativeResources {
    a04_rgx_native_resource_image image={};
    a04_rgx_render_buffer held[14]={};unsigned count=0,phase=0;
    a04_rgx_native_resource_io io={};
    void dropCold(){while(count){--count;io.drop(io.context,&held[count]);}}
    static void parts(a04_rgx_render_buffer out[14],const a04_rgx_render_resources *r,
        const a04_rgx_bif *heaps) {
        out[0]=r->bank;out[1]=heaps[0].pc;
        for(unsigned n=0;n<4;n++){out[2+3*n]=heaps[n].pd;out[3+3*n]=heaps[n].pt;out[4+3*n]=heaps[n].data;}
    }
    A04BR3RogueNativeResources(const A04BR3RogueNativeResources &)=delete;
    A04BR3RogueNativeResources &operator=(const A04BR3RogueNativeResources &)=delete;
public:
    A04BR3RogueNativeResources()=default;
    ~A04BR3RogueNativeResources(){if(phase!=2)dropCold();}
    bool prepared()const{return phase==1;} /* CPU preparation only */
    bool pinned()const{return phase==2;}
    bool resetCold(){if(phase==2)return false;dropCold();a04_rgx_fw_zero(&image,sizeof(image));
        a04_rgx_fw_zero(held,sizeof(held));io={};phase=0;return true;}
    int stage(const a04_rgx_render_resources *r,size_t r_bytes,const a04_rgx_bif *heaps,
        size_t heap_bytes,const a04_rgx_native_resource_io &ops)
    {
        if(phase || !ops.hold || !ops.drop || !ops.clean || !ops.acquire)return 1;
        a04_rgx_native_resource_image made;
        if(a04_rgx_native_resource_build(&made,sizeof(made),r,r_bytes,heaps,heap_bytes))return 1;
        for(unsigned n=0;n<4;n++)if(r->control[n]->ui32ReadOffset ||
            r->control[n]->ui32WriteOffset || r->control[n]->ui32DepOffset)return 1;
        if(*r->sync)return 1;
        a04_rgx_render_buffer all[14];parts(all,r,heaps);
        if(a04_rgx_fw_alias(this,sizeof(*this),r,sizeof(*r)) ||
           a04_rgx_fw_alias(this,sizeof(*this),heaps,heap_bytes))return 1;
        for(const auto &b:all)if(a04_rgx_fw_alias(this,sizeof(*this),b.cpu,b.bytes) ||
            (ops.context && a04_rgx_fw_alias(ops.context,1,b.cpu,b.bytes)))return 1;
        io=ops;
        for(const auto &b:all){if(io.hold(io.context,&b)){dropCold();phase=3;return 2;}held[count++]=b;}
        /* Tables/bank already built by caller. Initialize private data only:
         * GENERAL has a real shared36 island, never treat it as a guard. */
        for(unsigned h=0;h<4;h++)for(unsigned n=0;n<65536;n++)((uint8_t *)heaps[h].data.cpu)[n]=0xa5;
        for(unsigned n=0;n<36;n++)((uint8_t *)heaps[0].data.cpu)[8192+n]=made.background.shared[n];
        for(unsigned n=0;n<sizeof(made.pds);n++)((uint8_t *)heaps[1].data.cpu)[n]=made.pds[n];
        for(unsigned n=0;n<sizeof(made.usc);n++)((uint8_t *)heaps[2].data.cpu)[n]=made.usc[n];
        for(unsigned n=0;n<128;n++)((uint8_t *)heaps[3].data.cpu)[n]=made.isp.control[n];
        for(unsigned n=0;n<88;n++)((uint8_t *)heaps[3].data.cpu)[128+n]=made.isp.quad[n];
        for(const auto &b:all)if(io.clean(io.context,&b)){dropCold();phase=3;return 3;}
        image=made;phase=1;return 0;
    }
    /* verifyCold keeps the owner releasable: no FW exposure is permitted.
     * Source re-derivation and actual prepared-image verification, not a
     * rendering permission. Returned object has no full208/backgroundUSC.
     * Pin conservatively quarantines all14 allocation retains: no proven live
     * context/MMU teardown exists, including after a hypothetical UFO. */
    const a04_rgx_native_resource_image *verifyCold(const a04_rgx_render_resources *r,
        size_t r_bytes,const a04_rgx_bif *heaps,size_t heap_bytes)
    {
        if(phase!=1 || io.acquire(io.context))return nullptr;
        a04_rgx_native_resource_image expected;
        if(a04_rgx_native_resource_build(&expected,sizeof(expected),r,r_bytes,heaps,heap_bytes))return nullptr;
        a04_rgx_render_buffer all[14];parts(all,r,heaps);
        for(unsigned n=0;n<14;n++)if(!a04_rgx_native_same_buffer(all[n],held[n]))return nullptr;
        for(unsigned n=0;n<4;n++)if(r->control[n]->ui32ReadOffset || r->control[n]->ui32WriteOffset || r->control[n]->ui32DepOffset)return nullptr;
        if(*r->sync)return nullptr;
        for(unsigned h=0;h<4;h++)for(unsigned n=0;n<65536;n++) {
            uint8_t wanted=0xa5;
            if(h==0 && n>=8192 && n<8228)wanted=expected.background.shared[n-8192];
            if(h==1 && n<sizeof(expected.pds))wanted=expected.pds[n];
            if(h==2 && n<sizeof(expected.usc))wanted=expected.usc[n];
            if(h==3 && n<128)wanted=expected.isp.control[n];
            if(h==3 && n>=128 && n<216)wanted=expected.isp.quad[n-128];
            if(((volatile uint8_t *)heaps[h].data.cpu)[n]!=wanted)return nullptr;
        }
        /* Return a freshly re-derived image, not stale/private-padding data.
         * Staged bytes above must match it before any resource pin is exposed. */
        image=expected;return &image;
    }
    const a04_rgx_native_resource_image *pinImages(const a04_rgx_render_resources *r,
        size_t r_bytes,const a04_rgx_bif *heaps,size_t heap_bytes)
    {
        const auto *verified=verifyCold(r,r_bytes,heaps,heap_bytes);
        if(verified)phase=2;
        return verified;
    }
};
#endif
