/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Owns native context + BIF resources. Integration point: AFTER memory.stage,
 * BEFORE Runtime.prepare promotes code PTEs. No existing driver modification.
 * The source A04BR3RogueMemory must outlive this owner.
 * Cold MIPS mappings only; no live TLB mutation, no hardware register writes.
 */
#ifndef A04BR3_ROGUE_RENDER_MEMORY_H
#define A04BR3_ROGUE_RENDER_MEMORY_H
#include "A04BR3RogueMemory.h"
#include "A04BR3RogueRenderSubmit.h"
#include "A04BR3RogueHeapMap.h"
class A04BR3RogueRenderMemory {
    IOBufferMemoryDescriptor *owned[11]={};bool prepared[11]={};
    a04_rgx_render_resources resources={};a04_rgx_render_ring_state state={};
    a04_rgx_render_observation terminal={};
    uint32_t *mips=nullptr;bool mapped=false,pinned=false;
    bool extraHeaps=false; a04_rgx_bif heapViews[2]={};
    static unsigned size(unsigned i) {return (i==0 || i==4 || i==7 || i==10)?65536:4096;}
    A04BR3RogueRenderMemory(const A04BR3RogueRenderMemory &)=delete;
    A04BR3RogueRenderMemory &operator=(const A04BR3RogueRenderMemory &)=delete;
public:
    A04BR3RogueRenderMemory()=default;
    ~A04BR3RogueRenderMemory(){reset();}
    void pinForFirmware() {if(mapped)pinned=true;}
    void reset() {
        /* Existing Runtime.prepare promotes code to RW before FW release.
         * Conservatively quarantine if that phase began, even if our caller
         * omitted pinForFirmware(). There is no safe live teardown yet. */
        if (mapped && mips && (mips[0]&4U)) pinned=true;
        if (mapped && !pinned) {
            a04_rgx_fw_zero(mips+A04_RGX_RENDER_FW_OFFSET/4096,16*4);
            flush_dcache((vm_offset_t)mips,32768,0);
            __asm__ volatile("dsb sy":::"memory");
        }
        for (unsigned i=0;i<11;i++) {
            if (owned[i] && !pinned) {if(prepared[i])owned[i]->complete();owned[i]->release();}
            owned[i]=nullptr;prepared[i]=false;
        }
        mapped=false;mips=nullptr;extraHeaps=false;a04_rgx_fw_zero(heapViews,sizeof(heapViews));a04_rgx_fw_zero(&resources,sizeof(resources));
        a04_rgx_fw_zero(&state,sizeof(state));a04_rgx_fw_zero(&terminal,sizeof(terminal));
    }
    bool prepareCold(A04BR3RogueMemory &source,uint32_t first_context_id,uint64_t pdsVA=0,uint64_t uscVA=0) {
        if (pinned) return false;
        reset();
        if ((!pdsVA)!=(!uscVA)) return false;
        extraHeaps=pdsVA && uscVA;
        void *baseCPU[3];uint64_t basePA[3];
        if(!source.stagedMemory(baseCPU,basePA))return false;
        auto table=(uint32_t *)baseCPU[0];
        /* Active/Runtime-prepared candidates have writable code PTEs. */
        for(unsigned n=0;n<25;n++)
            if(table[n]!=(uint32_t)((basePA[1]+n*4096)>>6|0x13))return false;
        a04_rgx_render_buffer buffers[11]={};
        for(unsigned i=0;i<(extraHeaps?11U:5U);i++) {
            owned[i]=IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMemoryMapperNone|
                ((i==0 || i==4 || i==7 || i==10)?kIOMapWriteCombineCache:0),size(i),0xfffff000ULL);
            if(!owned[i] || owned[i]->prepare()!=kIOReturnSuccess){reset();return false;}
            prepared[i]=true;IOByteCount length=0;
            buffers[i]={owned[i]->getBytesNoCopy(),owned[i]->getPhysicalSegment(0,&length),size(i)};
            if(length<size(i) || !a04_rgx_render_buffer_valid(buffers[i])){reset();return false;}
            for(unsigned n=0;n<3;n++) {
                a04_rgx_render_buffer original={baseCPU[n],basePA[n],n==0?32768U:(n==1?102400U:20480U)};
                if(a04_rgx_render_buffers_overlap(buffers[i],original)){reset();return false;}
            }
            for(unsigned n=0;n<i;n++)
                if(a04_rgx_render_buffers_overlap(buffers[i],buffers[n])){reset();return false;}
        }
        a04_rgx_bif bif={buffers[1],buffers[2],buffers[3],buffers[4],A04_RGX_RENDER_SCRATCH_VA,65536,false};
        if(a04_rgx_render_build(&resources,&buffers[0],&bif,first_context_id)){reset();return false;}
        if(extraHeaps) {
            heapViews[0]={buffers[1],buffers[5],buffers[6],buffers[7],pdsVA,65536,false};
            heapViews[1]={buffers[1],buffers[8],buffers[9],buffers[10],uscVA,65536,true};
            const a04_rgx_bif heaps[]={bif,heapViews[0],heapViews[1]};
            if(a04_rgx_heap_map_build(heaps,3)){reset();return false;}
        }
        if(a04_rgx_render_map_cold(&resources,table,8192,basePA[0])){reset();return false;}
        mips=table;mapped=true;
        for(unsigned i=1;i<(extraHeaps?11U:5U);i++) {
            if(i==4 || i==7 || i==10)a04_rgx_fw_zero(buffers[i].cpu,buffers[i].bytes);
            else flush_dcache((vm_offset_t)buffers[i].cpu,buffers[i].bytes,0);
        }
        flush_dcache((vm_offset_t)table,32768,0);
        __asm__ volatile("dsb sy":::"memory");
        return true;
    }
    /* Trusted parent integration must verify FW ready/ABI and hold the same
     * KCCB/power mutex as GP SYNC + stop. This method never changes BIF MMIO.
     * The kernel view is the real Runtime layout, not a userspace descriptor. */
    int update(a04_rgx_fw_runtime &kernel,const a04_rgx_command_io &io,unsigned dm,uint32_t priority=0xffffffffU,bool transfer=false) {
        if(!mapped || !pinned)return A04_CCB_OFFLINE;
        return a04_rgx_render_submit_update(&resources,&state,&kernel,&io,dm,&terminal,priority,transfer);
    }
    const a04_rgx_bif *executionHeap(unsigned n)const {return mapped && extraHeaps && n<2?&heapViews[n]:nullptr;}
    unsigned ownedBytes()const {return mapped?143360U+(extraHeaps?147456U:0U):0U;}
    const a04_rgx_render_observation &observation() const {return terminal;}
    const a04_rgx_render_resources *view() const {return mapped?&resources:nullptr;}
};
#endif
