/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026 A04BR3 contributors.
 * Independent owned solid PROGRAMS, not wired to the default clear.
 * See .sources.json. No API exposes a complete208 task.
 * Original sub-producers establish bytes/addresses, not the end-to-end
 * zero-source shader-enabled combination: legal producer routing remains open.
 */
#ifndef A04BR3_ROGUE_SOLID_PROGRAMS_H
#define A04BR3_ROGUE_SOLID_PROGRAMS_H
#include "A04BR3RogueTransferFill.h"
#include "A04BR3RogueSolidShader.h"
#include "A04BR3RogueSharedLoad.h"
#include "A04BR3RoguePrimaryNop.h"
#include "A04BR3RogueFragmentKick.h"
#include "A04BR3RogueTransferBackground.h"
enum { A04_RGX_SOLID_PDS_BYTES=192,A04_RGX_SOLID_USC_BYTES=128,
       A04_RGX_SOLID_FRAGMENT_PDS=64,A04_RGX_SOLID_SHARED_PDS=128,
       A04_RGX_SOLID_FRAGMENT_USC=64,A04_RGX_SOLID_PRIMARY_USC=96 };
struct a04_rgx_solid_program_image {
    a04_rgx_pixel_event pixel; uint8_t eot[12];
    a04_rgx_tq_background_fields background; /* partial fields, NOT a task */
    uint8_t pds[A04_RGX_SOLID_PDS_BYTES],usc[A04_RGX_SOLID_USC_BYTES];
    uint32_t expected_pixel;
};
/* Resource metadata and ALL eleven backing allocations must outlive both
 * construction and any firmware use. Heaps are exact BRN65273 A8/BA as INIT;
 * CPU/PA disjointness and common PC are checked by the existing native clear
 * builder. Actual allocation ownership is retained by the adapter below.
 * This function only generates CPU bytes. No target/program/queue writes.
 */
static inline int a04_rgx_solid_program_build(a04_rgx_solid_program_image *out,size_t out_bytes,
    const a04_rgx_render_resources *r,const a04_rgx_bif *pds,const a04_rgx_bif *usc,
    uint32_t packed)
{
    if(out_bytes<sizeof(*out) || !a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
       !a04_rgx_fw_pointer(r,sizeof(*r)) || !a04_rgx_fw_pointer(pds,sizeof(*pds)) ||
       !a04_rgx_fw_pointer(usc,sizeof(*usc)) || packed==0xa5a5a5a5U ||
       a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)) ||
       a04_rgx_fw_alias(out,sizeof(*out),pds,sizeof(*pds)) ||
       a04_rgx_fw_alias(out,sizeof(*out),usc,sizeof(*usc)))return 1;
    a04_rgx_solid_program_image made; a04_rgx_fw_zero(&made,sizeof(made));
    a04_rgx_tq_clear base;
    if(a04_rgx_tq_clear_build(&base,r,pds,usc))return 1;
    made.pixel=base.pixel;for(unsigned n=0;n<12;n++)made.eot[n]=base.usc[n];
    const a04_rgx_bif heaps[]={r->bif,*pds,*usc};
    for(auto &h:heaps) {
        const a04_rgx_render_buffer parts[]={h.pc,h.pd,h.pt,h.data};
        for(auto &b:parts)if(a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes))return 1;
    }
    if(a04_rgx_fw_alias(out,sizeof(*out),r->bank.cpu,r->bank.bytes))return 1;
    uint8_t constants[16]={};a04_rgx_tq_put32(constants,packed);
    a04_rgx_solid_shader shader; a04_rgx_fragment_kick fragment; a04_rgx_shared_load shared;
    /* One packed32 fragment output, shared0. Other common-store words zero.
     * Source-defined register copy, not a floating-point colour conversion. */
    if(a04_rgx_solid_shader_compile(&shader,sizeof(shader),constants,4,0,4,1,0) ||
       a04_rgx_fragment_kick_build(&fragment,sizeof(fragment),64,shader.code_bytes,shader.temps,0) ||
       a04_rgx_shared_load_build(&shared,sizeof(shared),constants,16,0,4))return 1;
    for(unsigned n=0;n<64;n++) {
        made.pds[n]=made.pixel.image[n];
        made.pds[64+n]=fragment.image[n];made.pds[128+n]=shared.image[n];
    }
    for(unsigned n=0;n<sizeof(made.eot);n++)made.usc[n]=made.eot[n];
    for(unsigned n=0;n<shader.code_bytes;n++)made.usc[64+n]=shader.code[n];
    /* Native dedicated primary is independent of the solid fragment and EOT.
     * Keep all three entries source-built and separately addressable. */
    if(a04_rgx_primary_nop_generate(made.usc+96,sizeof(made.usc)-96))return 1;
    /* Native13ca3c clear secondary branch: first data8 DWORDs, second0.
     * Native13c794: IF0 DOUTW/DOUTW.END/HALT, temps1; code at shared+32.
     * Native129f88 exact packing, with empty second data explicitly zero.
     * General Background helper intentionally requires two nonempty blocks;
     * this is only the exact one-block arithmetic, NOT proof of a legal task. */
    a04_rgx_tq_background_fields bg={};
    bg.shader_branch_raw00=0x200;
    bg.raw38=UINT64_C(64)|(UINT64_C(160)<<32);
    bg.raw40=0;bg.raw48=128;
    bg.raw50=(UINT64_C(8)<<21)|(UINT64_C(1)<<55)|1;
    /* Deliberately return only proposed background fields. Native129f88
     * never enables this shader path for source_count0. Native12934c instead
     * needs an ISP primitive/control-list and a dedicated USC entry. Until
     * that entry contract is established these fields MUST NOT be patched
     * onto default-clear208 and no complete208 packet is exposed here. */
    made.background=bg;
    made.expected_pixel=packed;
    *out=made;return 0;
}
/* Adapter contract: hold() retains the exact allocation owning CPU/PA span;
 * drop() balances it; hold/clean return0 on success. clean() synchronously cleans CPU writes and executes a
 * store barrier. No fake cache success is supplied by this source unit.
 * All callbacks run while the SAME external producer/power/lifecycle mutex is
 * held. They must not publish FW work or mutate resource metadata. Mappings
 * must be private/cold and NC/WC as required by existing RenderSubmit. */
struct a04_rgx_solid_program_io {
    void *context;
    int (*hold)(void *,const a04_rgx_render_buffer *);
    void (*drop)(void *,const a04_rgx_render_buffer *);
    int (*clean)(void *,const a04_rgx_render_buffer *);
};
class A04BR3RogueSolidPrograms {
    a04_rgx_solid_program_image image={};
    a04_rgx_render_buffer held[11]={};unsigned count=0;
    a04_rgx_solid_program_io io={};
    unsigned phase=0; /* empty0, prepared1, FW-pinned2, failed3 */
    void dropCold() {while(count){--count;io.drop(io.context,&held[count]);}}
    A04BR3RogueSolidPrograms(const A04BR3RogueSolidPrograms &)=delete;
    A04BR3RogueSolidPrograms &operator=(const A04BR3RogueSolidPrograms &)=delete;
public:
    A04BR3RogueSolidPrograms()=default;
    ~A04BR3RogueSolidPrograms(){if(phase!=2)dropCold();}
    bool programsPrepared()const{return phase==1;}
    bool pinned()const{return phase==2;}
    /* No release of live mappings, even after a successful UFO. The current
     * firmware context holds its page catalogue until proven teardown/reset.
     * Retained allocation references are deliberately quarantined on pin. */
    bool resetCold() {
        if(phase==2)return false;
        dropCold();a04_rgx_fw_zero(&image,sizeof(image));
        a04_rgx_fw_zero(held,sizeof(held));io={};phase=0;return true;
    }
    int stage(const a04_rgx_render_resources *r,const a04_rgx_bif *pds,
        const a04_rgx_bif *usc,uint32_t packed,const a04_rgx_solid_program_io &ops)
    {
        if(phase || !ops.hold || !ops.drop || !ops.clean)return 1;
        a04_rgx_solid_program_image made;
        if(!a04_rgx_fw_pointer(r,sizeof(*r)) || !a04_rgx_render_view_valid(r))return 1;
        /* Cold bank only: refuse previously queued/active contexts instead
         * of overwriting programs under existing live/default-clear work. */
        for(unsigned n=0;n<4;n++)if(r->control[n]->ui32ReadOffset ||
            r->control[n]->ui32WriteOffset || r->control[n]->ui32DepOffset)return 1;
        if(*r->sync)return 1;
        if(a04_rgx_solid_program_build(&made,sizeof(made),r,pds,usc,packed))return 1;
        const a04_rgx_render_buffer parts[]={r->bank,r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data,
            pds->pd,pds->pt,pds->data,usc->pd,usc->pt,usc->data};
        if(a04_rgx_fw_alias(this,sizeof(*this),r,sizeof(*r)) ||
           a04_rgx_fw_alias(this,sizeof(*this),pds,sizeof(*pds)) ||
           a04_rgx_fw_alias(this,sizeof(*this),usc,sizeof(*usc)))return 1;
        for(auto &b:parts) {
            if(a04_rgx_fw_alias(this,sizeof(*this),b.cpu,b.bytes) ||
               (ops.context && a04_rgx_fw_alias(ops.context,1,b.cpu,b.bytes)))return 1;
        }
        io=ops;
        for(auto &b:parts) {
            if(io.hold(io.context,&b)){dropCold();phase=3;return 2;}
            held[count++]=b;
        }
        /* No firmware-visible task or queue write before successful
         * clean of programs, output guard poison, bank and all page tables. */
        for(unsigned n=0;n<sizeof(made.pds);n++)((uint8_t *)pds->data.cpu)[n]=made.pds[n];
        for(unsigned n=0;n<sizeof(made.usc);n++)((uint8_t *)usc->data.cpu)[n]=made.usc[n];
        for(unsigned n=0;n<r->bif.data.bytes;n++)((uint8_t *)r->bif.data.cpu)[n]=0xa5;
        for(auto &b:parts)if(io.clean(io.context,&b)){dropCold();phase=3;return 3;}
        image=made;phase=1;return 0;
    }
    /* Pin the owned program resources under producer mutex.
     * Re-derive full source image + verify actual heap bytes, no user-supplied
     * shader/task accepted. NULL on any changed resource/program/target.
     * Returned image is held by this object's required lifetime, along with
     * allocation retains. No complete208 or submission API is exposed here:
     * a legal producer route and then a bounded hardware trial remain required. */
    const a04_rgx_solid_program_image *pinPrograms(const a04_rgx_render_resources *r,
        const a04_rgx_bif *pds,const a04_rgx_bif *usc)
    {
        if(phase!=1)return nullptr;
        a04_rgx_solid_program_image expected;
        if(a04_rgx_solid_program_build(&expected,sizeof(expected),r,pds,usc,image.expected_pixel))return nullptr;
        const a04_rgx_render_buffer parts[]={r->bank,r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data,
            pds->pd,pds->pt,pds->data,usc->pd,usc->pt,usc->data};
        for(unsigned n=0;n<11;n++)if(parts[n].cpu!=held[n].cpu || parts[n].pa!=held[n].pa || parts[n].bytes!=held[n].bytes)return nullptr;
        /* Compare defined fields only, never C++ aggregate padding. */
        if(image.background.raw38!=expected.background.raw38 ||
           image.background.raw40!=expected.background.raw40 ||
           image.background.raw48!=expected.background.raw48 ||
           image.background.raw50!=expected.background.raw50 ||
           image.background.shader_branch_raw00!=expected.background.shader_branch_raw00 ||
           image.background.reserved!=expected.background.reserved)return nullptr;
        for(unsigned n=0;n<64;n++)if(image.pixel.image[n]!=expected.pixel.image[n])return nullptr;
        for(unsigned n=0;n<12;n++)if(image.eot[n]!=expected.eot[n])return nullptr;
        for(unsigned n=0;n<sizeof(image.pds);n++)if(image.pds[n]!=expected.pds[n] || ((volatile uint8_t *)pds->data.cpu)[n]!=image.pds[n])return nullptr;
        for(unsigned n=0;n<sizeof(image.usc);n++)if(image.usc[n]!=expected.usc[n] || ((volatile uint8_t *)usc->data.cpu)[n]!=image.usc[n])return nullptr;
        for(unsigned n=0;n<r->bif.data.bytes;n++)if(((volatile uint8_t *)r->bif.data.cpu)[n]!=0xa5)return nullptr;
        phase=2;return &image;
    }
};
#endif
