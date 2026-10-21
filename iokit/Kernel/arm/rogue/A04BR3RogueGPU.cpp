/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * PowerVR Rogue kernel transport bring-up; not a rendering accelerator yet.
 * No user GPU mappings, command submission, firmware upload or DMA enabled.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
#include "A04BR3RoguePower.h"
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
class A04BR3RogueGPU : public IOService {
    OSDeclareDefaultStructors(A04BR3RogueGPU);
public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
private:
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
    vm_offset_t core=ml_io_map(0x13000000,0x1000);if(!core)return false;
    uint32_t lo=*(volatile uint32_t *)(core+0x20),hi=*(volatile uint32_t *)(core+0x24);
    uint64_t bvnc=((uint64_t)hi<<32)|lo;
    __asm__ volatile("dmb oshld":::"memory");
    IOLog("A04BR3 GPU: BVNC=%u.%u.%u.%u raw=0x%llx; firmware/MMU/submission not enabled\n",hi>>16,hi&65535,lo>>16,lo&65535,bvnc);
    if(!bvnc || bvnc==~0ULL)return false;
    setProperty("PowerVRBVNC",bvnc,64);setProperty("Model","MT8167 PowerVR Rogue");registerService();return true;
}
