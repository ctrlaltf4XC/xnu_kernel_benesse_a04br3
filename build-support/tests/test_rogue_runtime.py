# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'


class FirmwareRuntime(unittest.TestCase):
    def test_layout_allocator_mappings_and_transactional_bounds(self):
        code=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueRuntimeLayout.h"
static unsigned char mainbuf[A04_RGX_MAIN_BYTES] __attribute__((aligned(4096)));
static unsigned char cfg[A04_RGX_CONFIG_BYTES] __attribute__((aligned(4096)));
static uint32_t table[8192],saved[8192];
static void *fw(struct a04_rgx_fw_runtime *r,uint32_t addr) {
 if(addr>=0xc1ff0000U)return r->config.cpu+(addr-0xc1ff0000U);
 assert(addr>=0xc0040000U && addr<0xc0080000U);return r->main.cpu+(addr-0xc0040000U);
}
int main(void){
 struct a04_rgx_fw_runtime r;
 assert(!a04_rgx_fw_runtime_init(&r,mainbuf,0x80000000,cfg,0x81000000,26000000));
 assert((void*)r.init==cfg && !r.init->bFirmwareStarted && !r.init->sRGXCompChecks.bUpdated);
 assert(r.main.used==90176 && r.config.used==sizeof(RGXFWIF_INIT));
 assert(r.init->sOSConfig.ui32Addr && r.init->sTraceBufCtl.ui32Addr && r.init->sHWPerfCtl.ui32Addr);
 assert(fw(&r,r.init->sOSConfig.ui32Addr)==r.osconfig);
 assert(fw(&r,r.init->sTraceBufCtl.ui32Addr)==r.trace);
 assert(fw(&r,r.init->sRGXFWIfHWRInfoBufCtl.ui32Addr)==r.hwr);
 assert(fw(&r,r.init->sRuntimeCfg.ui32Addr)==r.runtime);
 assert(fw(&r,r.osconfig->sPowerSync.ui32Addr)==r.power_sync && !*r.power_sync);
 assert(r.kctl->ui32WrapMask==127 && r.kctl->ui32CmdSize==56);
 assert(r.fctl->ui32WrapMask==31 && r.fctl->ui32CmdSize==40);
 assert(!r.kctl->ui32ReadOffset && !r.kctl->ui32WriteOffset);
 RGXFWIF_CCB_CTL *checkpoint=fw(&r,r.init->psCheckpointCCBCtl.ui32Addr);
 assert(checkpoint->ui32WrapMask==1023 && checkpoint->ui32CmdSize==4);
 assert(r.trace->ui32TraceBufSizeInDWords==12000 && r.trace->ui32LogType==3);
 assert(fw(&r,r.trace->sTraceBuf[0].pui32RGXFWIfTraceBuffer.ui32Addr)==r.trace->sTraceBuf[0].pui32TraceBuffer);
 assert(r.init->sFaultPhysAddr.uiAddr==0x80015000 && r.init->ui32MarkerVal==1);
 assert(r.init->sPDSExecBase.uiAddr==0xa800000000ULL && r.init->sUSCExecBase.uiAddr==0xba00000000ULL);
 assert(!a04_rgx_mips_map(table,8192,0,0x70000000,0x19000,0,1));
 assert(!a04_rgx_mips_map(table,8192,0x32000,0x71000000,0x5000,1,0));
 assert(!a04_rgx_fw_runtime_map(table,8192,0x80000000,0x81000000));
 unsigned valid=0;for(unsigned i=0;i<8192;i++)if(table[i])valid++;
 assert(valid==110 && table[0x40]==0x42000017 && table[0x1ff0]==0x42040017);
 assert(table[0x1ff0+260/4096] && !table[0x18+1] && !table[0x80]);
 memcpy(saved,table,sizeof(table));
 assert(a04_rgx_fw_runtime_map(table,8192,0x80000000,0x81000000));
 assert(!memcmp(saved,table,sizeof(table)));
 assert(a04_rgx_fw_runtime_init(&r,mainbuf,0xfffff000,cfg,0x81000000,26000000));
 assert(a04_rgx_fw_runtime_init(&r,mainbuf,0x80000000,cfg,0x80001000,26000000));
 struct a04_rgx_fw_pool p={mainbuf,0x80000000,64,63,0x40000};
 RGXFWIF_DEV_VIRTADDR address={0xa5};
 assert(!a04_rgx_fw_alloc(&p,8,&address) && p.used==63 && address.ui32Addr==0xa5);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.c').write_text(code)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(GPU),str(p/'test.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

    def test_owned_runtime_prepare_executor_timeout_and_quarantine(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'IOKit').mkdir();(p/'arm').mkdir()
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text(r'''
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
using IOByteCount=uint64_t;using vm_offset_t=uintptr_t;
static int kernel_task=0;
enum {A04_TEST_MAIN_BYTES=0x40000,kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,kIOMapWriteCombineCache=0x400,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor;
static IOBufferMemoryDescriptor *retained[128];
static bool renderAllocating;
static unsigned descriptors,attempts,live,completed,cleaned;static int allocFault;
class IOBufferMemoryDescriptor {
public:
 void *cpu;uint64_t size,pa;unsigned id;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(int,unsigned options,uint64_t size,uint64_t){
  assert(((options&kIOMapWriteCombineCache)!=0)==(size==A04_TEST_MAIN_BYTES || (renderAllocating && (attempts==0 || attempts==4))));
  unsigned slot=attempts++;
  if(allocFault==(int)slot+1)return nullptr;
  auto b=new IOBufferMemoryDescriptor;b->id=descriptors++;assert(b->id<128);
  b->pa=0x80000000ULL+b->id*0x100000ULL;b->size=size;
  assert(!posix_memalign(&b->cpu,16384,size));memset(b->cpu,0xcc,size);
  retained[b->id]=b;++live;return b;
 }
 int prepare(){return 0;}void complete(){++completed;}
 void release(){retained[id]=nullptr;free(cpu);delete this;--live;}
 uint64_t getPhysicalSegment(int,IOByteCount *length){*length=size;return pa;}
 void *getBytesNoCopy(){return cpu;}
};
''')
            (p/'arm/caches_internal.h').write_text('#pragma once\nstatic void flush_dcache(vm_offset_t,unsigned,int){++cleaned;}\n')
            (p/'A04BR3RogueMemory.h').write_text(r'''
#pragma once
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <arm/caches_internal.h>
#include "A04BR3RogueMipsMMU.h"
static uint32_t baseTable[8192] __attribute__((aligned(16384)));
static char baseCode[0x19000] __attribute__((aligned(16384)));
static char baseData[0x5000] __attribute__((aligned(16384)));
class A04BR3RogueMemory {
public:
 bool owned=false;
 A04BR3RogueMemory(){memset(baseTable,0,sizeof(baseTable));
  assert(!a04_rgx_mips_map(baseTable,8192,0,0x70000000,0x19000,0,1));
  assert(!a04_rgx_mips_map(baseTable,8192,0x32000,0x71000000,0x5000,1,0));}
 bool stagedMemory(void **cpu,uint64_t *pa){
  cpu[0]=baseTable;cpu[1]=baseCode;cpu[2]=baseData;
  pa[0]=0x72000000;pa[1]=0x70000000;pa[2]=0x71000000;return true;}
 void markHardwareOwned(){owned=true;}
};
''')
            header=(GPU/'A04BR3RogueRuntime.h').read_text()
            for asm in ('__asm__ volatile("dsb sy":::"memory");',
                        '__asm__ volatile("dmb ishld":::"memory");',
                        '__asm__ volatile("dsb sy; dmb ishld":::"memory");'):
                header=header.replace(asm,'__asm__ volatile("":::"memory");')
            header=header.replace('__asm__ volatile("dc ivac, %0"::"r"(line):"memory");','(void)line;')
            (p/'A04BR3RogueRuntime.h').write_text(header)
            render=(GPU/'A04BR3RogueRenderMemory.h').read_text().replace(
                '__asm__ volatile("dsb sy":::"memory");','__asm__ volatile("":::"memory");')
            (p/'A04BR3RogueRenderMemory.h').write_text(render)
            (p/'test.cpp').write_text(r'''
#include "A04BR3RogueRuntime.h"
#include "A04BR3RogueRenderMemory.h"
static A04BR3RogueRenderMemory *activeRender;
static uint64_t regs[0x10000/8];static A04BR3RogueRuntime *active;
static bool released;static unsigned writes,waited;static int failure;
static uint32_t read32(void *,uint32_t at){assert(at<0x10000);return ((uint32_t*)regs)[at/4];}
static uint64_t read64(void *,uint32_t at){assert(at<0x10000);return regs[at/8];}
static void write32(void *,uint32_t at,uint32_t value){assert(at<0x10000);++writes;((uint32_t*)regs)[at/4]=value;}
static void write64(void *,uint32_t at,uint64_t value){assert(at<0x10000);++writes;regs[at/8]=value;
 if(at==0x100 && value==0)released=true;}
static void waitUS(void *,uint32_t value){waited+=value;assert(waited<=1010009);}
static uint32_t shared(void *,uint32_t address){
 if(released && failure!=1){
  auto init=const_cast<RGXFWIF_INIT *>(active->init());init->bFirmwareStarted=(IMG_BOOL)(failure==2?2:1);
  auto &c=init->sRGXCompChecks;c.bUpdated=IMG_TRUE;c.ui32DDKVersion=0x1000b;
  c.ui32DDKBuild=failure==3?1:5425693;c.ui32FWProcessorVersion=1;
  c.sFWBVNC.ui64BVNC=c.sHWBVNC.ui64BVNC=0x001600280036001eULL;
 }
 return active->readShared(address);
}
static unsigned commandKicks,commandWaits;static bool commandAnswer=true,commandCorrupt;
static void *commandMemory(uint32_t address) {
 assert(address>=0xc0040000 && address<0xc0080000);
 return (char*)retained[descriptors-3]->cpu+(address-0xc0040000);
}
static void publishCommand(void *);
static void acquireCommand(void *){}
static void kickCommand(void *,uint32_t at,uint32_t value) {
 assert(at==0xb00 && value==0x10);++commandKicks;
}
static void waitCommand(void *,uint32_t us) {
 assert(us==10);++commandWaits;
 if(!commandAnswer)return;
 if(commandCorrupt){activeRender->view()->control[1]->ui32ReadOffset=4096;return;}
 auto init=active->init();
 auto ctl=(RGXFWIF_CCB_CTL*)commandMemory(init->psKernelCCBCtl.ui32Addr);
 auto commands=(RGXFWIF_KCCB_CMD*)commandMemory(init->psKernelCCB.ui32Addr);
 auto &cmd=commands[ctl->ui32ReadOffset];
 if(cmd.eCmdType==RGXFWIF_KCCB_CMD_SYNC) {
  assert(cmd.eDM==3);
  *(uint32_t*)commandMemory(cmd.uCmdData.sSyncData.sSyncObjDevVAddr.ui32Addr)=cmd.uCmdData.sSyncData.uiUpdateVal;
 } else {
  assert(cmd.eCmdType==RGXFWIF_KCCB_CMD_KICK && (cmd.eDM==2 || cmd.eDM==3));
  auto r=activeRender->view();unsigned n=cmd.eDM==3;
  assert(cmd.uCmdData.sCmdKickData.psContext.ui32Addr==
    a04_rgx_render_fw_address(r,n?&r->context->s3DContext:&r->context->sTAContext));
  auto client=r->control[n];
  auto header=(RGXFWIF_CCB_CMD_HEADER*)(r->commands[n]+client->ui32ReadOffset);
  assert(header->eCmdType==A04_RGX_CLIENT_UPDATE && header->ui32CmdSize==8);
  auto ufo=(RGXFWIF_UFO*)((char*)header+40);
  assert(ufo->puiAddrUFO.ui32Addr==r->sync_addr.ui32Addr);
  *r->sync=ufo->ui32Value;
  client->ui32ReadOffset=client->ui32WriteOffset=client->ui32DepOffset=cmd.uCmdData.sCmdKickData.ui32CWoffUpdate;
 }
 ctl->ui32ReadOffset=ctl->ui32WriteOffset;
}
static a04_rgx_command_io commandIO={nullptr,publishCommand,acquireCommand,kickCommand,waitCommand};
static void publishCommand(void *) {
 /* Reentrant producer cannot interleave a second command. */
 assert(active->fence(3,commandIO)==A04_CCB_BUSY);
 if(activeRender)assert(active->renderUpdate(*activeRender,2,commandIO)==A04_CCB_BUSY);
}
int main(){
 for(allocFault=1;allocFault<=3;allocFault++) {
  attempts=0;A04BR3RogueMemory source;A04BR3RogueRuntime runtime;
  unsigned baseline=live;assert(!runtime.prepare(source,26000000));assert(live==baseline);
  assert(!baseTable[0x40] && !baseTable[0x1ff0]);
 }
 allocFault=0;attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRuntime runtime;
  assert(runtime.prepare(source,26000000));assert(baseTable[0x40] && baseTable[0x1ff0]);
  assert(baseTable[0]&4U);
  runtime.reset();assert(!live && !baseTable[0x40] && !baseTable[0x1ff0] && !(baseTable[0]&4U));}
 for(failure=0;failure<6;failure++) {
  A04BR3RogueMemory source;A04BR3RogueRuntime runtime;A04BR3RogueRenderMemory render;
  active=&runtime;activeRender=&render;attempts=0;renderAllocating=true;
  assert(render.prepareCold(source,1));renderAllocating=false;attempts=0;
  commandKicks=commandWaits=0;commandAnswer=true;commandCorrupt=false;
  released=false;writes=waited=0;memset(regs,0,sizeof(regs));
  assert(runtime.prepare(source,26000000));assert(!runtime.ready());
  a04_rgx_exec_io io={nullptr,read32,read64,write32,write64,waitUS,shared};
  unsigned pinned=live;
  assert(runtime.renderUpdate(render,2,commandIO)==A04_CCB_OFFLINE);
  render.pinForFirmware();int result=runtime.start(source,io);
  assert(released && source.owned && writes);
  if(failure==1)assert(waited==1000009); /* exact finite MTK 1s poll, plus startup waits and recovery reset */
  if(!failure || failure>=4){assert(result==0 && runtime.ready()==1);
   assert(runtime.fence(4,commandIO)==A04_CCB_INPUT && !commandKicks);
   assert(runtime.fence(3,commandIO)==A04_CCB_OK && commandKicks==1 && commandWaits==1);
   assert(runtime.renderUpdate(render,4,commandIO)==A04_CCB_INPUT && commandKicks==1);
   assert(runtime.renderUpdate(render,2,commandIO)==A04_CCB_OK);
   assert(runtime.renderUpdate(render,3,commandIO)==A04_CCB_OK);
   assert(runtime.fence(3,commandIO)==A04_CCB_OK);
   commandAnswer=failure==4;commandCorrupt=failure==4;commandWaits=0;
   int commandResult=failure==5?runtime.fence(3,commandIO):runtime.renderUpdate(render,3,commandIO);
   assert(commandResult==(failure==4?A04_CCB_CORRUPT:A04_CCB_TIMEOUT));
   assert(commandKicks==5 && commandWaits==(failure==4?1U:10000U));
   assert(runtime.fence(3,commandIO)==A04_CCB_OFFLINE && commandKicks==5);
   assert(runtime.renderUpdate(render,2,commandIO)==A04_CCB_OFFLINE && commandKicks==5);
  }
  else {assert(result!=0);assert(regs[0x100/8]==0x100000000ULL);
   assert(runtime.fence(3,commandIO)==A04_CCB_OFFLINE);}
  assert(!runtime.prepare(source,26000000));
  render.reset();runtime.reset();assert(live==pinned); /* never free fabric-visible resources */
 }
 /* Test harness alone reclaims quarantined mock resources after mock device
  * shutdown. Production deliberately retains them until a real power cycle. */
 for(unsigned i=0;i<descriptors;i++)if(retained[i]){retained[i]->complete();retained[i]->release();}
 assert(!live && completed==descriptors);
}
''')
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(p),'-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
