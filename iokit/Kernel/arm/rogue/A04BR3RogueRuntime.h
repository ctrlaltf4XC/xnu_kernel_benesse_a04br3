/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Owns runtime DMA memory and drives the bounded first FW boot attempt.
 * All allocations remain pinned until reboot after any hardware start attempt.
 * CPU interrupt is deliberately masked until the platform SPI dispatcher exists;
 * wrapper events are polled/acknowledged without stealing GICC_IAR from XNU.
 */
#ifndef A04BR3_ROGUE_RUNTIME_H
#define A04BR3_ROGUE_RUNTIME_H
#include "A04BR3RogueMemory.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueStartupExecutor.h"
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueRenderMemory.h"
class A04BR3RogueRuntime {
    IOBufferMemoryDescriptor *buffers[3]={};
    bool prepared[3]={};bool mapped=false,pinned=false;
    void *baseCPU[3]={};uint64_t basePA[3]={};
    void *cpu[3]={};uint64_t pa[3]={};
    a04_rgx_fw_runtime layout={};
    a04_rgx_command_state commandState={};
    unsigned commandBusy=0;bool firmwareOnline=false;
    static unsigned bytes(unsigned i) {
        return i==0?A04_RGX_MAIN_BYTES:(i==1?A04_RGX_CONFIG_BYTES:0x4000);
    }
    A04BR3RogueRuntime(const A04BR3RogueRuntime &)=delete;
    A04BR3RogueRuntime &operator=(const A04BR3RogueRuntime &)=delete;
public:
    A04BR3RogueRuntime()=default;
    ~A04BR3RogueRuntime(){reset();}
    void reset() {
        firmwareOnline=false;
        if (mapped && !pinned) {
            uint32_t *table=(uint32_t *)baseCPU[0];
            for (unsigned n=0;n<0x19000/4096;n++) table[n]&=~4U;
            a04_rgx_fw_zero(table+A04_RGX_MAIN_OFFSET/4096,A04_RGX_MAIN_BYTES/4096*4);
            a04_rgx_fw_zero(table+A04_RGX_CONFIG_OFFSET/4096,A04_RGX_CONFIG_BYTES/4096*4);
            flush_dcache((vm_offset_t)table,0x8000,0);
        }
        mapped=false;
        for (unsigned i=0;i<3;i++) {
            if (buffers[i] && !pinned) {
                if (prepared[i]) buffers[i]->complete();
                buffers[i]->release();
            }
            buffers[i]=nullptr;prepared[i]=false;
        }
        /* Pinned descriptors retain their original reference, intentionally.
         * No fabric-drain/power-off proof => no DMA memory reuse this boot. */
        a04_rgx_fw_zero(&layout,sizeof(layout));
    }
    bool prepare(A04BR3RogueMemory &source,uint32_t clock_hz) {
        if (pinned) return false;
        reset();
        if (!source.stagedMemory(baseCPU,basePA)) return false;
        for (unsigned i=0;i<3;i++) {
            buffers[i]=IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMemoryMapperNone|
                (i==0?kIOMapWriteCombineCache:0),
                bytes(i),i==2?0xffffc000ULL:0xfffff000ULL);
            if (!buffers[i] || buffers[i]->prepare()!=kIOReturnSuccess) { reset();return false; }
            prepared[i]=true;
            IOByteCount length=0;
            pa[i]=buffers[i]->getPhysicalSegment(0,&length);cpu[i]=buffers[i]->getBytesNoCopy();
            if (length<bytes(i) || !a04_rgx_mips_physical_range(pa[i],bytes(i),i==2?0x4000:4096) ||
                !a04_rgx_fw_pointer(cpu[i],bytes(i)) || ((uintptr_t)cpu[i]&63) ||
                a04_rgx_start_overlap(pa[i],bytes(i),0x1fc00000,0x4000)) { reset();return false; }
            for (unsigned j=0;j<3;j++) {
                unsigned baseBytes=j==0?0x8000:(j==1?0x19000:0x5000);
                if (a04_rgx_start_overlap(pa[i],bytes(i),basePA[j],baseBytes) ||
                    a04_rgx_fw_alias(cpu[i],bytes(i),baseCPU[j],baseBytes)) { reset();return false; }
            }
            for (unsigned j=0;j<i;j++)
                if (a04_rgx_start_overlap(pa[i],bytes(i),pa[j],bytes(j)) ||
                    a04_rgx_fw_alias(cpu[i],bytes(i),cpu[j],bytes(j))) { reset();return false; }
            a04_rgx_fw_zero(cpu[i],bytes(i));
        }
        /* CPU stage supplied RX code entries. Validate them before promotion:
         * rgxinit.c RGXInitAllocFWImgMem explicitly marks code GPU_WRITEABLE. */
        uint32_t *table=(uint32_t *)baseCPU[0];
        for (unsigned n=0;n<0x19000/4096;n++)
            if (table[n]!=(uint32_t)((basePA[1]+n*4096)>>6|0x13)) {reset();return false;}
        if (a04_rgx_fw_runtime_init(&layout,cpu[0],pa[0],cpu[1],pa[1],clock_hz) ||
            a04_rgx_fw_runtime_map((uint32_t *)baseCPU[0],8192,pa[0],pa[1])) { reset();return false; }
        mapped=true;
        for (unsigned n=0;n<0x19000/4096;n++) table[n]|=4U;
        /* Main runtime uses normal noncache CPU memory: no cache-line sharing
         * hazards on live CCB controls/UFOs. Config remains read-only on CPU. */
        for (unsigned i=1;i<3;i++) flush_dcache((vm_offset_t)cpu[i],bytes(i),0);
        flush_dcache((vm_offset_t)baseCPU[0],0x8000,0);
        __asm__ volatile("dsb sy":::"memory");
        return true;
    }
    static void invalidate(void *buffer,unsigned bytes) {
        /* A35 D-cache line64; all owned allocations and per-object allocations
         * are at least64 aligned, with no concurrent CPU writer during boot. */
        vm_offset_t first=(vm_offset_t)buffer&~63ULL;
        vm_offset_t end=((vm_offset_t)buffer+bytes+63)&~63ULL;
        for (vm_offset_t line=first;line<end;line+=64)
            __asm__ volatile("dc ivac, %0"::"r"(line):"memory");
        __asm__ volatile("dsb sy; dmb ishld":::"memory");
    }
    uint32_t readShared(uint32_t physical) {
        if (!mapped || physical<pa[1] || physical-pa[1]>A04_RGX_CONFIG_BYTES-4 || (physical&3)) return 0xffffffffU;
        void *p=(unsigned char *)cpu[1]+(physical-pa[1]);
        invalidate(p,4);
        uint32_t value=*(volatile uint32_t *)p;
        __asm__ volatile("dmb ishld":::"memory");
        return value;
    }
    int start(A04BR3RogueMemory &source,const a04_rgx_exec_io &io) {
        if (!mapped || pinned || !io.shared32) return A04_EXEC_INPUT;
        a04_rgx_start_input input={};
        input.bvnc=0x001600280036001eULL;
        input.code_pa=basePA[1];input.data_pa=basePA[2];input.table_pa=basePA[0];
        input.trampoline_pa=pa[2];input.init_pa=pa[1];input.init_bytes=A04_RGX_CONFIG_BYTES;
        input.started_offset=offsetof(RGXFWIF_INIT,bFirmwareStarted);
        input.ready_initial=readShared((uint32_t)(pa[1]+input.started_offset));
        input.strict_same_address_ordering=1;input.device_pa0_valid=0;
        /* MTK sysinfo.h workqueue profile: 1s / 20000 samples. */
        input.poll_budget=20000;input.poll_delay_us=50;
        /* prepare proves owned ranges/ABI/CCBs/full mappings/cache publication.
         * Driver must have powered exact BVNC, mapped64KiB registers, and masked
         * INTID217 for the bounded polled route before calling this method. */
        input.evidence=A04_START_ALL;
        struct a04_rgx_start_plan plan;
        if (a04_rgx_start_plan(&input,&plan)!=A04_START_OK) return A04_EXEC_INPUT;
        pinned=true;source.markHardwareOwned();
        uint32_t irq=0;
        int error=a04_rgx_execute_start(&input,&io,&irq);
        if (!error) {
            for (unsigned tries=0;tries<1000;tries++) {
                uint32_t updated=readShared((uint32_t)(pa[1]+offsetof(RGXFWIF_INIT,sRGXCompChecks)+
                    offsetof(RGXFWIF_COMPCHECKS,bUpdated)));
                if (updated==1) break;
                if (tries==999) {error=4;break;}
                io.wait_us(io.context,10);
            }
            invalidate(layout.init,sizeof(*layout.init));
            const RGXFWIF_COMPCHECKS &c=layout.init->sRGXCompChecks;
            if (!error && (c.ui32DDKVersion!=0x1000b || c.ui32DDKBuild!=5425693 ||
                c.ui32FWProcessorVersion!=1 || c.sFWBVNC.ui32LayoutVersion!=3 ||
                c.sFWBVNC.ui64BVNC!=input.bvnc || c.sHWBVNC.ui64BVNC!=input.bvnc)) error=5;
        }
        if (irq&1) io.write32(io.context,0x8b0,1);
        if (error) {
            /* Quarantine memory regardless of reset readback. Never recycle
             * buffers merely because a core reset write was posted. */
            io.write64(io.context,0x100,0x100000000ULL);
            (void)io.read64(io.context,0x100);
            io.wait_us(io.context,3);
        }
        firmwareOnline=!error;
        return error;
    }
    /* Trusted kernel fence bridge for GP/TA/3D. No arbitrary KICK payloads.
     * Caller must keep power fixed and serialize service stop with this call. */
    int fence(uint32_t dm,const a04_rgx_command_io &io) {
        unsigned expected=0;
        if (!__atomic_compare_exchange_n(&commandBusy,&expected,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))
            return A04_CCB_BUSY;
        int error=firmwareOnline?a04_rgx_command_fence(&layout,&commandState,&io,dm,10000,10):A04_CCB_OFFLINE;
        __atomic_store_n(&commandBusy,0,__ATOMIC_RELEASE);
        return error;
    }
    /* Only a source-owned render owner is accepted; never expose the mutable
     * kernel layout to a bridge client. Same producer gate as fence(), with
     * outer service lock required for power/stop exclusion. Optional priority
     * selects only bounded native TA/3D PRIORITY0/1; no task payload input. */
    int renderUpdate(A04BR3RogueRenderMemory &owner,uint32_t dm,const a04_rgx_command_io &io,uint32_t priority=0xffffffffU,bool transfer=false) {
        unsigned expected=0;
        if (!__atomic_compare_exchange_n(&commandBusy,&expected,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))
            return A04_CCB_BUSY;
        int error=firmwareOnline && !commandState.poisoned?
            owner.update(layout,io,dm,priority,transfer):A04_CCB_OFFLINE;
        /* An unresolved KICK owns the shared transport too. Do not allow SYNC
         * to overwrite/replay it after timeout or corrupt firmware response. */
        if (error==A04_CCB_TIMEOUT || error==A04_CCB_CORRUPT) commandState.poisoned=true;
        __atomic_store_n(&commandBusy,0,__ATOMIC_RELEASE);
        return error;
    }
    const RGXFWIF_INIT *init() const {return layout.init;}
    uint32_t readyPhysical() const {return (uint32_t)(pa[1]+offsetof(RGXFWIF_INIT,bFirmwareStarted));}
    uint32_t ready() {return readShared(readyPhysical());}
};
#endif
