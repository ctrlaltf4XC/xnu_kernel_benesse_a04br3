/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026 A04BR3 contributors.
 * Real low4GiB IOBuffer allocator/cache/retain adapter for NativeResources.
 * Inert cold CPU preparation only: no FW MIPS mapping, MMIO, full208 or KICK.
 * Explicit ISP window is caller-selected within compiled BRN heap0..1GiB.
 * Thread context only, never IRQ. Caller must pin BEFORE making these
 * mappings/bank accessible to firmware. Only an explicit CPU-only driver trial is installed; no Memory/FW hookup.
 */
#ifndef A04BR3_ROGUE_NATIVE_BUFFERS_H
#define A04BR3_ROGUE_NATIVE_BUFFERS_H
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/task.h>
#include <arm/caches_internal.h>
#include "A04BR3RogueNativeResources.h"
class A04BR3RogueNativeBuffers {
    IOBufferMemoryDescriptor *owned[14]={};bool prepared[14]={};
    a04_rgx_render_buffer buffers[14]={};
    a04_rgx_render_resources resources={};a04_rgx_bif heaps[4]={};
    A04BR3RogueNativeResources images;bool staged=false;
    static unsigned bytes(unsigned i){return (i==0 || i==4 || i==7 || i==10 || i==13)?65536:4096;}
    static void barrier() {
#if defined(__aarch64__) || defined(__arm64__)
        __asm__ volatile("dsb sy":::"memory");
#else
        __sync_synchronize(); /* host source tests only; no hardware model */
#endif
    }
    int index(const a04_rgx_render_buffer &b)const {
        for(unsigned i=0;i<14;i++)if(owned[i] && a04_rgx_native_same_buffer(b,buffers[i]))return (int)i;
        return -1;
    }
    int find(const a04_rgx_render_buffer &b) const {
        int i=index(b);if(i<0 || !prepared[i])return -1;
        IOByteCount length=0;
        if(owned[i]->getBytesNoCopy()!=b.cpu || owned[i]->getPhysicalSegment(0,&length)!=b.pa || length<b.bytes)return -1;
        return i;
    }
    static int hold(void *ctx,const a04_rgx_render_buffer *b) {
        auto self=(A04BR3RogueNativeBuffers *)ctx;int i=self->find(*b);if(i<0)return 1;
        self->owned[i]->retain();return 0;
    }
    static void drop(void *ctx,const a04_rgx_render_buffer *b) {
        auto self=(A04BR3RogueNativeBuffers *)ctx;
        /* Balance the exact retained OBJECT even when a later segment
         * revalidation failed. Cold rollback must not leak that reference. */
        int i=self->index(*b);if(i>=0)self->owned[i]->release();
    }
    static int clean(void *ctx,const a04_rgx_render_buffer *b) {
        auto self=(A04BR3RogueNativeBuffers *)ctx;if(self->find(*b)<0)return 1;
        flush_dcache((vm_offset_t)b->cpu,b->bytes,0);barrier();return 0;
    }
    static int acquire(void *){barrier();return 0;}
    A04BR3RogueNativeBuffers(const A04BR3RogueNativeBuffers &)=delete;
    A04BR3RogueNativeBuffers &operator=(const A04BR3RogueNativeBuffers &)=delete;
public:
    A04BR3RogueNativeBuffers()=default;
    ~A04BR3RogueNativeBuffers(){if(!images.pinned())resetCold();}
    bool resetCold() {
        if(images.pinned() || !images.resetCold())return false;
        for(unsigned i=0;i<14;i++)if(owned[i]) {
            if(prepared[i])owned[i]->complete();owned[i]->release();owned[i]=nullptr;prepared[i]=false;
        }
        staged=false;a04_rgx_fw_zero(buffers,sizeof(buffers));
        a04_rgx_fw_zero(heaps,sizeof(heaps));a04_rgx_fw_zero(&resources,sizeof(resources));return true;
    }
    bool prepareCold(uint32_t first_context_id,uint64_t isp_window_va) {
        if(images.pinned())return false;
        resetCold();
        if(!first_context_id || first_context_id>0xfffffffbU || (isp_window_va&65535) ||
           isp_window_va>A04_RGX_ISP_HEAP_BYTES-65536)return false;
        for(unsigned i=0;i<14;i++) {
            unsigned size=bytes(i);
            owned[i]=IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMemoryMapperNone|
                (size==65536?kIOMapWriteCombineCache:0),size,0xfffff000ULL);
            if(!owned[i] || owned[i]->prepare()!=kIOReturnSuccess){resetCold();return false;}
            prepared[i]=true;IOByteCount length=0;
            buffers[i]={owned[i]->getBytesNoCopy(),owned[i]->getPhysicalSegment(0,&length),size};
            if(length<size || !a04_rgx_render_buffer_valid(buffers[i])){resetCold();return false;}
            for(unsigned j=0;j<i;j++)if(a04_rgx_render_buffers_overlap(buffers[i],buffers[j])){resetCold();return false;}
        }
        const uint64_t va[]={UINT64_C(0x65c0000000),UINT64_C(0xa800000000),UINT64_C(0xba00000000),isp_window_va};
        for(unsigned n=0;n<4;n++)heaps[n]={buffers[1],buffers[2+3*n],buffers[3+3*n],buffers[4+3*n],va[n],65536,n==2};
        if(a04_rgx_render_build(&resources,&buffers[0],&heaps[0],first_context_id) ||
           a04_rgx_heap_map_build(heaps,4)){resetCold();return false;}
        const a04_rgx_native_resource_io io={this,hold,drop,clean,acquire};
        if(images.stage(&resources,sizeof(resources),heaps,sizeof(heaps),io)){resetCold();return false;}
        staged=true;return true;
    }
    const a04_rgx_render_resources *view()const{return staged?&resources:nullptr;}
    const a04_rgx_bif *heapViews()const{return staged?heaps:nullptr;}
    unsigned ownedBytes()const{return staged?364544U:0U;}
    bool pinned()const{return images.pinned();}
    const a04_rgx_native_resource_image *verifyCold() {
        if(!staged)return nullptr;
        for(unsigned i=0;i<14;i++)if(find(buffers[i])<0)return nullptr;
        return images.verifyCold(&resources,sizeof(resources),heaps,sizeof(heaps));
    }
    /* CPU-only opt-in: no FW mappings/context registration or task submission.
     * ISP window0x10000 and context IDs are private test values, never exposed
     * to hardware. All allocations are released before return, even on failure.
     * Return0 skipped,1 verified/released,2 failed,3 wrong target or busy owner. */
    unsigned coldTrial(unsigned enabled,uint64_t bvnc) {
        if(enabled!=1)return 0;
        if(bvnc!=UINT64_C(0x001600280036001e) || staged || images.pinned())return 3;
        bool ok=prepareCold(1,UINT64_C(0x10000));
        if(ok)ok=verifyCold()!=nullptr;
        bool released=resetCold();
        return ok && released?1:2;
    }
    const a04_rgx_native_resource_image *pinImages() {
        if(!staged)return nullptr;
        for(unsigned i=0;i<14;i++)if(find(buffers[i])<0)return nullptr;
        return images.pinImages(&resources,sizeof(resources),heaps,sizeof(heaps));
    }
    /* After pin, root prepares/references AND the14 additional lease retains
     * deliberately survive destruction. No live teardown proof exists. This
     * is not a driver attachment or permission to run missing backgroundUSC. */
};
#endif
