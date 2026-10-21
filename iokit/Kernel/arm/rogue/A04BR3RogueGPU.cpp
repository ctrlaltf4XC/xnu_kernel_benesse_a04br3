/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * PowerVR Rogue kernel transport bring-up; not a rendering accelerator yet.
 * No user GPU mappings or rendering command submission. Source-owned firmware runtime
 * is started only from the explicitly supplied matching boot payload.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <arm/machine_routines.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include <pexpert/arm64/boot.h>
#include "A04BR3RoguePower.h"
#include "A04BR3RogueMemory.h"
#include "A04BR3RogueBootFirmware.h"
#include "A04BR3RogueRuntime.h"
#include "A04BR3RogueRenderMemory.h"
#include "A04BR3RogueNativeBuffers.h"
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t);
/* Platform SLEH can call this hook before its unhandled-SPI panic, then EOI.
 * Current platform has no SPI dispatcher: do NOT unmask INTID217 until wired. */
static vm_offset_t a04_rogue_irq_core;
extern "C" int a04br3_rogue_irq_dispatch(uint32_t intid) {
    vm_offset_t core=__atomic_load_n(&a04_rogue_irq_core,__ATOMIC_ACQUIRE);
    if (intid!=217 || !core) return 0;
    uint32_t status=*(volatile uint32_t *)(core+0x8a8);
    if (status==0xffffffffU) return 0;
    if (status&1) *(volatile uint32_t *)(core+0x8b0)=1;
    __asm__ volatile("dsb sy":::"memory");
    return 1;
}
class A04BR3RogueGPU : public IOService {
    OSDeclareDefaultStructors(A04BR3RogueGPU);
public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    using IOService::callPlatformFunction;
    /* Kernel rendering coordinator fence entry. param1 is the scalar DM
     * (0 GP,2 TA,3 3D); all other parameters must be null. Synchronous,
     * thread context only; no userspace ABI, command memory or output pointer.
     * Success means only that this DM executed an owned SYNC, not a render.
     */
    IOReturn callPlatformFunction(const OSSymbol *name,bool wait,
        void *domain,void *p2,void *p3,void *p4) APPLE_KEXT_OVERRIDE {
        if (!name) return kIOReturnBadArgument;
        if (!name->isEqualTo("A04BR3RogueFence"))
            return IOService::callPlatformFunction(name,wait,domain,p2,p3,p4);
        uintptr_t dm=(uintptr_t)domain;
        if (p2 || p3 || p4 || (dm!=0 && dm!=2 && dm!=3)) return kIOReturnBadArgument;
        if (ml_at_interrupt_context()) return kIOReturnNotPermitted;
        int error=submitFence((uint32_t)dm);
        if (!error) return kIOReturnSuccess;
        if (error==A04_CCB_TIMEOUT) return kIOReturnTimeout;
        if (error==A04_CCB_BUSY || error==A04_CCB_FULL) return kIOReturnBusy;
        if (error==A04_CCB_OFFLINE) return kIOReturnNotReady;
        return kIOReturnError;
    }
    void free() APPLE_KEXT_OVERRIDE {
        if (bridgeLock) {IOLockFree(bridgeLock);bridgeLock=nullptr;}
        IOService::free();
    }
    void stop(IOService *provider) APPLE_KEXT_OVERRIDE {
        if (bridgeLock) IOLockLock(bridgeLock);
        bridgeOnline=false;
        setProperty("FirmwareRunning",kOSBooleanFalse);
        __atomic_store_n(&a04_rogue_irq_core,(vm_offset_t)0,__ATOMIC_RELEASE);
        if (core && attempted) {
            *(volatile uint64_t *)(core+0x100)=0x100000000ULL;
            __asm__ volatile("dsb sy":::"memory");
            (void)*(volatile uint64_t *)(core+0x100);
        }
        render.reset();runtime.reset();memory.reset();
        if (bridgeLock) IOLockUnlock(bridgeLock);
        IOService::stop(provider);
    }
private:
    A04BR3RogueNativeBuffers nativeTrial; /* private CPU-only opt-in; never FW mapped */
    A04BR3RogueMemory memory;
    A04BR3RogueRuntime runtime;
    A04BR3RogueRenderMemory render; /* destroyed before runtime/base mappings */
    vm_offset_t core=0,gic=0;
    bool attempted=false;
    IOLock *bridgeLock=nullptr;bool bridgeOnline=false,renderReported=false;
    static uint32_t fwRead32(void *p,uint32_t offset) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        uint32_t value=*(volatile uint32_t *)(s->core+offset);
        __asm__ volatile("dmb oshld":::"memory");return value;
    }
    static uint64_t fwRead64(void *p,uint32_t offset) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        uint64_t value=*(volatile uint64_t *)(s->core+offset);
        __asm__ volatile("dmb oshld":::"memory");return value;
    }
    static void fwWrite32(void *p,uint32_t offset,uint32_t value) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        *(volatile uint32_t *)(s->core+offset)=value;
        __asm__ volatile("dsb oshst":::"memory");
    }
    static void fwWrite64(void *p,uint32_t offset,uint64_t value) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        *(volatile uint64_t *)(s->core+offset)=value;
        __asm__ volatile("dsb oshst":::"memory");
    }
    static void fwWait(void *,uint32_t us){IODelay(us);}
    static uint32_t fwShared(void *p,uint32_t physical) {
        return ((A04BR3RogueGPU *)p)->runtime.readShared(physical);
    }
    static void commandPublish(void *) {__asm__ volatile("dsb sy":::"memory");}
    static void commandAcquire(void *) {__asm__ volatile("dmb oshld":::"memory");}
    int submitFence(uint32_t dm) {
        if (!bridgeLock) return A04_CCB_OFFLINE;
        if (!IOLockTryLock(bridgeLock)) return A04_CCB_BUSY;
        a04_rgx_command_io io={this,commandPublish,commandAcquire,fwWrite32,fwWait};
        int error=bridgeOnline?runtime.fence(dm,io):A04_CCB_OFFLINE;
        IOLockUnlock(bridgeLock);
        return error;
    }
    int submitRenderUpdate(uint32_t dm,uint32_t priority=0xffffffffU,bool transfer=false) {
        if (!bridgeLock) return A04_CCB_OFFLINE;
        if (!IOLockTryLock(bridgeLock)) return A04_CCB_BUSY;
        a04_rgx_command_io io={this,commandPublish,commandAcquire,fwWrite32,fwWait};
        uint64_t timerBefore=fwRead64(this,0x160); /* pinned RGX_CR_TIMER */
        int error=bridgeOnline?runtime.renderUpdate(render,dm,io,priority,transfer):A04_CCB_OFFLINE;
        uint64_t timerAfter=fwRead64(this,0x160);
        const auto &s=render.observation();
        if (error && s.valid && !renderReported) {
            renderReported=true;
            IOLog("A04BR3 GPU: render terminal DM=%u transfer=%u polls=%u KCCB r/w/expected=%u/%u/%u client r/w/dep/expected=%u/%u/%u/%u UFO=%u expected=%u\n",
                s.dm,s.transfer,s.polls,s.kread,s.kwrite,s.kexpected,s.cread,s.cwrite,s.cdep,s.cexpected,s.ufo,s.expected);
            IOLog("A04BR3 GPU: render FWCCB r/w/type=%u/%u/0x%x context flags=0x%x priority=%u expected=%u seq=%u run=0x%x/0x%x wait=0x%x/0x%x\n",
                s.fread,s.fwrite,s.ftype,s.flags,s.priority,s.priorityExpected,s.prioritySequence,s.runPrev,s.runNext,s.waitPrev,s.waitNext);
            IOLog("A04BR3 GPU: render FW executed=%u traceptr=%u power=%u HWR=0x%x faults=%u startedTime=%u idleTime=0x%llx CRtimer=0x%llx/0x%llx; terminal snapshot only, no replay\n",
                s.kexecuted,s.tracePointer,s.power,s.hwr,s.fwfaults,s.startedTime,
                (unsigned long long)s.idleTime,(unsigned long long)timerBefore,(unsigned long long)timerAfter);
        }
        IOLockUnlock(bridgeLock);
        return error;
    }
    uint32_t verifyRenderBridge() {
        if (!render.view()) return 0;
        uint32_t verified=0;
        /* Only the matching DDK GP KickSync contract is established for a
         * payload-free NULL+UPDATE. Failed DM2 standalone UPDATE is not
         * replayed, nor is the GP NULL task assumed valid on TA/3D. */
        for (uint32_t dm=0;dm<1;dm++) {
            int error=submitRenderUpdate(dm);
            setProperty("FirmwareRenderUpdateResult",(uint64_t)error,32);
            IOLog("A04BR3 GPU: client UPDATE/KICK DM=%u result=%d; FW UFO + KCCB/client read-offset required, no draw packet\n",dm,error);
            if (error) break;
            /* A separate SYNC confirms the existing fence path still works
             * after firmware has consumed this client's UPDATE. */
            error=submitFence(dm);
            setProperty("FirmwareRenderPostFenceResult",(uint64_t)error,32);
            IOLog("A04BR3 GPU: post-UPDATE SYNC DM=%u result=%d\n",dm,error);
            if (error) break;
            verified|=1U<<dm;
        }
        setProperty("FirmwareRenderUpdateVerifiedDMs",(uint64_t)verified,32);
        return verified;
    }
    bool verifyPriorityBridge() {
        uint32_t verified=0;
        /* rgxta3d.c SetRenderContextPriorityKM explicitly calls the same
         * ContextSetPriority for TA then3D. No NULL task on either domain. */
        for (uint32_t dm=2;dm<=3;dm++) {
            for (uint32_t priority=0;priority<=1;priority++) {
                int error=submitRenderUpdate(dm,priority);
                setProperty("FirmwarePriorityResult",(uint64_t)error,32);
                IOLog("A04BR3 GPU: client PRIORITY/KICK DM=%u target=%u result=%d; FW priority + KCCB/client read-offset required, no UFO/draw proof\n",dm,priority,error);
                if (error) {setProperty("FirmwarePriorityVerifiedDMs",(uint64_t)verified,32);return false;}
            }
            int error=submitFence(dm);
            setProperty("FirmwarePriorityPostFenceResult",(uint64_t)error,32);
            IOLog("A04BR3 GPU: post-PRIORITY SYNC DM=%u result=%d\n",dm,error);
            if (error) break;
            verified|=1U<<dm;
        }
        setProperty("FirmwarePriorityVerifiedDMs",(uint64_t)verified,32);
        return verified==0xc;
    }
    bool verifyTransferContext() {
        /* rgxtransfer.c SetTransferContextPriorityKM explicitly uses DM3.
         * PRIORITY-only preflight; the separate owned clear follows on success. */
        for (uint32_t priority=0;priority<=1;priority++) {
            int error=submitRenderUpdate(3,priority,true);
            setProperty("FirmwareTQContextResult",(uint64_t)error,32);
            IOLog("A04BR3 GPU: independent TQ_3D PRIORITY target=%u result=%d; no transfer task submitted\n",priority,error);
            if (error) return false;
        }
        int error=submitFence(3);
        setProperty("FirmwareTQPostFenceResult",(uint64_t)error,32);
        setProperty("FirmwareTQContextVerified",error?kOSBooleanFalse:kOSBooleanTrue);
        IOLog("A04BR3 GPU: post-TQ-context SYNC DM=3 result=%d\n",error);
        return !error;
    }
    void verifyOwnedClear() {
        setProperty("FirmwareOwnedClearVerified",kOSBooleanFalse);
        if(!render.clearPrepared() || !bridgeLock || !IOLockTryLock(bridgeLock))return;
        a04_rgx_command_io io={this,commandPublish,commandAcquire,fwWrite32,fwWait};
        int error=bridgeOnline?runtime.renderClear(render,io):A04_CCB_OFFLINE;
        setProperty("FirmwareOwnedClearResult",(uint64_t)error,32);
        IOLog("A04BR3 GPU: owned TQ_3D clear32x32 result=%d zeroWords=%u/1024 guardErrors=%u; native0x22, UFO + CCB consumption + pixel readback required\n",
            error,render.clearedWords(),render.guardErrors());
        const auto &cache=render.readbackObservation();
        IOLog("A04BR3 GPU: clear readback SLC result=%d pixelsRead=%u beforeA5=%u/1024 afterA5=%u/1024 first=0x%x last=0x%x observed=%u KCCB=%u/%u/%u UFO=%u/%u FWCCB=%u/%u polls=%u\n",
            render.readbackResult(),(unsigned)render.readbackPixelsValid(),render.beforeA5(),render.afterA5(),render.firstWord(),render.lastWord(),
            (unsigned)cache.valid,cache.kread,cache.kwrite,cache.kexpected,cache.ufo,cache.expected,cache.fread,cache.fwrite,cache.polls);
        if(error && render.observation().valid) {
            const auto &s=render.observation();
            IOLog("A04BR3 GPU: clear terminal polls=%u KCCB=%u/%u/%u client=%u/%u/%u/%u UFO=%u/%u FWCCB=%u/%u/0x%x HWR=0x%x faults=%u\n",
                s.polls,s.kread,s.kwrite,s.kexpected,s.cread,s.cwrite,s.cdep,s.cexpected,s.ufo,s.expected,s.fread,s.fwrite,s.ftype,s.hwr,s.fwfaults);
        }
        IOLockUnlock(bridgeLock);
        if(!error) {
            int fence=submitFence(3);
            setProperty("FirmwareOwnedClearPostFenceResult",(uint64_t)fence,32);
            setProperty("FirmwareOwnedClearVerified",fence?kOSBooleanFalse:kOSBooleanTrue);
            IOLog("A04BR3 GPU: post-owned-clear SYNC3 result=%d; no iOS acceleration bridge\n",fence);
        }
    }
    uint32_t verifyCommandBridge() {
        const uint32_t domains[]={A04_RGX_DM_GP,A04_RGX_DM_TA,A04_RGX_DM_3D};
        uint32_t verified=0;
        for (unsigned i=0;i<3;i++) {
            int error=submitFence(domains[i]);
            setProperty("FirmwareFenceResult",(uint64_t)error,32);
            IOLog("A04BR3 GPU: KCCB SYNC DM=%u result=%d; FW UFO + read-offset required, no rendering submitted\n",domains[i],error);
            if (error) break;
            verified|=1U<<domains[i];
        }
        setProperty("FirmwareFenceVerifiedDMs",(uint64_t)verified,32);
        return verified;
    }
    void startFirmware() {
        unsigned enabled=1;
        PE_parse_boot_argn("a04br3.gpu_start",&enabled,sizeof(enabled));
        if (enabled!=1) return;
        if (!core) return;
        if (!bridgeLock) bridgeLock=IOLockAlloc();
        if (!bridgeLock) {setProperty("BringupStage","firmware-bridge-lock-failed");return;}
        /* Original mt8167.dtsi: GIC_SPI185 => INTID217, level low. Keep it
         * masked for the polling boot route; the current SLEH panics on SPIs.
         * This write owns exactly one bit and never consumes GICC_IAR. */
        gic=ml_io_map(0x10310000,0x1000);
        if (!gic) {setProperty("BringupStage","firmware-irq-map-failed");return;}
        *(volatile uint32_t *)(gic+0x180+(217/32)*4)=1U<<(217%32);
        __asm__ volatile("dsb sy":::"memory");
        if (*(volatile uint32_t *)(gic+0x100+(217/32)*4)&(1U<<(217%32))) {
            setProperty("BringupStage","firmware-irq-mask-failed");return;
        }
        setProperty("FirmwareIRQRoute","polled-gic-masked");
        /* clk-mt8167: csw_mux_mfg parent4=clk26m_ck, mfg_mm
         * parent8=csw_mux_mfg_sel, factor clk26m_ck is 1:1. The power
         * path selects these exact parents; verify readback before supplying
         * the actual core frequency to the native firmware ABI. */
        uint32_t mux0=readReg(this,A04_RGX_TOP,0x000);
        uint32_t mux1=readReg(this,A04_RGX_TOP,0x004);
        uint32_t slow=readReg(this,A04_RGX_TOP,0x040);
        if (((mux0>>8)&7)!=4 || ((mux1>>8)&63)!=8 ||
            (slow&((3U<<18)|(3U<<20)))) {
            setProperty("BringupStage","firmware-clock-readback-failed");
            IOLog("A04BR3 GPU: FW clock mux readback rejected 0x%x/0x%x/0x%x; MIPS not released\n",mux0,mux1,slow);
            return;
        }
        IOLog("A04BR3 GPU: FW core clock 26000000 Hz from verified crystal mux path\n");
        /* No live TLB edits: reserve/map native contexts before runtime
         * promotes code PTEs, and before MIPS reset is released. Allocation
         * failure leaves the proven firmware/SYNC path available. */
        bool renderPrepared=render.prepareCold(memory,1,0xa800000000ULL,0xba00000000ULL);
        setProperty("FirmwareRenderContextPrepared",renderPrepared?kOSBooleanTrue:kOSBooleanFalse);
        setProperty("FirmwareOwnedClearPrepared",render.clearPrepared()?kOSBooleanTrue:kOSBooleanFalse);
        IOLog("A04BR3 GPU: cold render context prepared=%u; no tasks/shaders submitted\n",(unsigned)renderPrepared);
        if (!runtime.prepare(memory,26000000)) {
            render.reset();
            setProperty("BringupStage","firmware-runtime-allocation-failed");
            IOLog("A04BR3 GPU: FW runtime allocation/mapping rejected; MIPS not released\n");return;
        }
        setProperty("FirmwareMemoryBytes",(uint64_t)(A04BR3RogueMemory::TableBytes+
            A04BR3RogueMemory::CodeBytes+A04BR3RogueMemory::DataBytes+
            A04_RGX_MAIN_BYTES+A04_RGX_CONFIG_BYTES+0x4000+(renderPrepared?render.ownedBytes():0)),32);
        a04_rgx_exec_io io={this,fwRead32,fwRead64,fwWrite32,fwWrite64,fwWait,fwShared};
        render.pinForFirmware(); /* quarantine before any reset release, including failed start */
        attempted=true;
        IOLog("A04BR3 GPU: FW start INIT=%u ready_offset=%u; bounded polled IRQ route, INTID217 masked\n",
            (unsigned)sizeof(RGXFWIF_INIT),(unsigned)offsetof(RGXFWIF_INIT,bFirmwareStarted));
        int error=runtime.start(memory,io);
        setProperty("FirmwareStartResult",(uint64_t)error,32);
        if (error) {
            setProperty("BringupStage","firmware-start-failed");
            IOLog("A04BR3 GPU: FW start failed code=%d ready=%u exception=0x%x; reset requested, allocations pinned\n",
                error,runtime.ready(),fwRead32(this,0x8d0));return;
        }
        __atomic_store_n(&a04_rogue_irq_core,core,__ATOMIC_RELEASE);
        setProperty("FirmwareRunning",kOSBooleanTrue);
        setProperty("BringupStage","firmware-ready");
        IOLog("A04BR3 GPU: FW ready=1 ABI/DDK/BVNC verified; IRQ=0x%x; no acceleration bridge\n",fwRead32(this,0x8a8));
        bridgeOnline=true;
        if (verifyCommandBridge()==0xd && verifyRenderBridge()==1 && verifyPriorityBridge() && verifyTransferContext()) verifyOwnedClear();
    }
    void stageBootFirmware() {
        DTEntry map=nullptr;
        const void *value=nullptr;unsigned size=0;
        setProperty("FirmwarePayloadStatus","absent");
        if (SecureDTLookupEntry(nullptr,"/chosen/memory-map",&map)!=kSuccess ||
            SecureDTGetProperty(map,"GPUFirmware-A04BR3",&value,&size)!=kSuccess) return;
        uint64_t asset[2]={},reserved[2]={};
        bool valid=value && size==sizeof(asset);
        if (valid) bcopy(value,asset,sizeof(asset));
        value=nullptr;size=0;
        valid=valid && SecureDTGetProperty(map,"Kernel-A04BR3",&value,&size)==kSuccess &&
            value && size==sizeof(reserved);
        if (valid) bcopy(value,reserved,sizeof(reserved));
        /* A firmware asset must never be interpreted as a root RAMDisk. */
        const void *ramdisk=nullptr;unsigned ramdiskSize=0;
        valid=valid && SecureDTGetProperty(map,"RAMDisk",&ramdisk,&ramdiskSize)!=kSuccess;
        const boot_args *ba=(const boot_args *)PE_state.bootArgs;
        valid=valid && ba && a04_rgx_boot_firmware_range(asset[0],asset[1],
            reserved[0],reserved[1],ba->physBase,ba->memSize,ba->topOfKernelData);
        vm_offset_t source=0;
        if (valid) {
            source=ml_static_ptovirt((vm_offset_t)asset[0]);
            valid=source && a04_rgx_fw_pointer((const void *)source,(size_t)asset[1]) &&
                ml_static_ptovirt((vm_offset_t)(asset[0]+asset[1]-1))==source+asset[1]-1;
        }
        if (!valid) {
            memory.reset();
            setProperty("FirmwarePayloadStatus","rejected-reservation");
            setProperty("BringupStage","firmware-input-invalid");
            IOLog("A04BR3 GPU: firmware input reservation rejected; no firmware execution\n");
            return;
        }
        int error=memory.stage((const void *)source,(size_t)asset[1]);
        setProperty("FirmwareStageResult",(uint64_t)error,32);
        setProperty("FirmwareRunning",kOSBooleanFalse);
        if (error) {
            setProperty("FirmwarePayloadStatus","rejected-image");
            setProperty("BringupStage","firmware-stage-failed");
            IOLog("A04BR3 GPU: CPU firmware stage rejected error=%d; no firmware execution\n",error);
        } else {
            setProperty("FirmwarePayloadStatus","cpu-staged");
            setProperty("BringupStage","firmware-cpu-staged");
            IOLog("A04BR3 GPU: reserved payload CPU-staged/cache cleaned bytes=%llu; GPU MMU/wrapper/execution disabled\n",asset[1]);
            startFirmware();
        }
    }
    vm_offset_t regs[4];
    static uint32_t readReg(void *p,unsigned b,unsigned o) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        uint32_t v=*(volatile uint32_t *)(s->regs[b]+o);
        __asm__ volatile("dmb oshld":::"memory");return v;
    }
    static void writeReg(void *p,unsigned b,unsigned o,uint32_t v) {
        A04BR3RogueGPU *s=(A04BR3RogueGPU *)p;
        *(volatile uint32_t *)(s->regs[b]+o)=v;
        __asm__ volatile("dsb oshst":::"memory");
    }
    static void waitUS(void *,unsigned us) { IODelay(us); }
};
OSDefineMetaClassAndStructors(A04BR3RogueGPU,IOService);
bool A04BR3RogueGPU::start(IOService *provider)
{
    unsigned enabled=0;
    if(!PE_parse_boot_argn("a04br3.display_probe",&enabled,sizeof(enabled)) || enabled!=1 || !IOService::start(provider))return false;
    regs[0]=ml_io_map(0x10006000,0x1000);regs[1]=ml_io_map(0x10001000,0x1000);
    regs[2]=ml_io_map(0x10000000,0x1000);regs[3]=ml_io_map(0x13ffe000,0x1000);
    if(!regs[0]||!regs[1]||!regs[2]||!regs[3])return false;
    a04_rgx_io io={this,readReg,writeReg,waitUS};
    IOLog("A04BR3 GPU: MFG status=0x%x/0x%x, starting Rogue power domains\n",readReg(this,0,0x60c),readReg(this,0,0x610));
    int error=a04_rgx_power_on(&io);
    setProperty("GPUAccelerationSupported",kOSBooleanFalse);
    setProperty("BringupStage",error?"power-failed":"register-access");
    if(error){IOLog("A04BR3 GPU: power stage=%d failed; no core access or DMA\n",error);return false;}
    core=ml_io_map(0x13000000,0x10000);if(!core)return false;
    uint32_t lo=*(volatile uint32_t *)(core+0x20),hi=*(volatile uint32_t *)(core+0x24);
    uint64_t bvnc=((uint64_t)hi<<32)|lo;
    __asm__ volatile("dmb oshld":::"memory");
    IOLog("A04BR3 GPU: BVNC=%u.%u.%u.%u raw=0x%llx; firmware/MMU/submission not enabled\n",hi>>16,hi&65535,lo>>16,lo&65535,bvnc);
    if(!bvnc || bvnc==~0ULL)return false;
    unsigned nativeTrialEnabled=0;
    if (PE_parse_boot_argn("a04br3.gpu_trial",&nativeTrialEnabled,sizeof(nativeTrialEnabled)) && nativeTrialEnabled==1) {
        unsigned result=nativeTrial.coldTrial(nativeTrialEnabled,bvnc);
        setProperty("NativeResourceCPUTrialResult",(uint64_t)result,32);
        IOLog("A04BR3 GPU: native CPU resource trial result=%u logicalBytes=%u; private buffers released, no FW mapping/task/pixel proof\n",
            result,result==1?364544U:0U);
    }
    /* This source-built display bring-up already requires display_probe=1.
     * Reserve inert buffers by default; the optional override can disable it.
     * Avoid requiring another boot argument in the bounded shim command line. */
    unsigned reserve=1;
    PE_parse_boot_argn("a04br3.gpu_memory_stage",&reserve,sizeof(reserve));
    if (reserve==1) {
        if (bvnc!=0x001600280036001eULL || !memory.reserve()) {
            setProperty("BringupStage","memory-reservation-failed");
            IOLog("A04BR3 GPU: inert memory reservation failed; firmware remains disabled\n");
        } else {
            setProperty("BringupStage","memory-reserved-empty");
            setProperty("FirmwareRunning",kOSBooleanFalse);
            setProperty("FirmwareMemoryBytes",(uint64_t)(A04BR3RogueMemory::TableBytes+
                A04BR3RogueMemory::CodeBytes+A04BR3RogueMemory::DataBytes),32);
            IOLog("A04BR3 GPU: low-4GiB empty memory reserved/cache cleaned; no firmware or MMU activation\n");
            stageBootFirmware();
        }
    }
    setProperty("PowerVRBVNC",bvnc,64);setProperty("Model","MT8167 PowerVR Rogue");registerService();return true;
}
