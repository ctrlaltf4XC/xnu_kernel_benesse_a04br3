# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class RogueRenderMemory(unittest.TestCase):
    def test_owned_cold_stage_failures_cache_mode_map_and_quarantine(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'IOKit').mkdir()
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text(r'''
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
using IOByteCount=uint64_t;using vm_offset_t=uintptr_t;
static int kernel_task;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,
 kIOMapWriteCombineCache=0x400,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor;
static IOBufferMemoryDescriptor *retained[512];static unsigned live,total,attempts,cleaned;
static unsigned failAllocation,failPrepare;
class IOBufferMemoryDescriptor {
public:void *cpu;uint64_t bytes,pa;unsigned id,slot;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(int,unsigned options,uint64_t size,uint64_t mask){
  assert(mask==0xfffff000);unsigned at=++attempts;if(at==failAllocation)return nullptr;
  assert(((options&kIOMapWriteCombineCache)!=0)==(size==65536));
  auto b=new IOBufferMemoryDescriptor;b->id=total++;b->slot=at;assert(b->id<512);
  b->bytes=size;b->pa=0x90000000ULL+b->id*0x20000;
  assert(!posix_memalign(&b->cpu,4096,size));memset(b->cpu,0xcc,size);retained[b->id]=b;++live;return b;
 }
 int prepare(){return slot==failPrepare?1:0;}void complete(){}
 void release(){free(cpu);retained[id]=nullptr;--live;delete this;}
 void *getBytesNoCopy(){return cpu;}
 uint64_t getPhysicalSegment(int,IOByteCount *length){*length=bytes;return pa;}
};
static void flush_dcache(vm_offset_t,unsigned,int){++cleaned;}
''')
            (p/'A04BR3RogueMemory.h').write_text(r'''
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "A04BR3RogueMipsMMU.h"
alignas(32768) static uint32_t sourceTable[8192];
alignas(4096) static char sourceCode[102400],sourceData[20480];
class A04BR3RogueMemory {
public:A04BR3RogueMemory(){memset(sourceTable,0,sizeof(sourceTable));
 assert(!a04_rgx_mips_map(sourceTable,8192,0,0x70000000,102400,0,1));}
 bool stagedMemory(void **cpu,uint64_t *pa) {
 cpu[0]=sourceTable;cpu[1]=sourceCode;cpu[2]=sourceData;
 pa[0]=0x72000000;pa[1]=0x70000000;pa[2]=0x71000000;return true;}
};
''')
            text=(GPU/'A04BR3RogueRenderMemory.h').read_text()
            text=re.sub(r'__asm__ volatile\([^;]*\);','',text)
            (p/'A04BR3RogueRenderMemory.h').write_text(text)
            (p/'test.cpp').write_text(r'''
#include "A04BR3RogueRenderMemory.h"
alignas(4096) static unsigned char mainbuf[A04_RGX_MAIN_BYTES],configbuf[A04_RGX_CONFIG_BYTES];
static a04_rgx_fw_runtime kernel;static a04_rgx_command_state transport;
static const a04_rgx_render_resources *view;
static unsigned mode,kicks,waits,published;
static uint32_t nextClient,sequence;
static void acquire(void *){}
static void publish(void *){++published;}
static void kick(void *,uint32_t reg,uint32_t value){
 assert(reg==0xb00 && value==0x10);++kicks;
 if(kicks==2){
  assert(published==4 && kernel.kctl->ui32ReadOffset==1 && kernel.kctl->ui32WriteOffset==3);
  RGXFWIF_KCCB_CMD expected={};expected.eCmdType=RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;expected.eDM=0;
  expected.uCmdData.sSLCFlushInvalData.bInval=IMG_TRUE;
  assert(!memcmp(&kernel.kcommands[1],&expected,56));
  expected={};expected.eCmdType=RGXFWIF_KCCB_CMD_SYNC;expected.eDM=0;
  expected.uCmdData.sSyncData.sSyncObjDevVAddr=kernel.command_sync_addr;
  expected.uCmdData.sSyncData.uiUpdateVal=1;
  assert(!memcmp(&kernel.kcommands[2],&expected,56));return;
 }
 assert(kicks==1 && published==2);
 auto &cmd=kernel.kcommands[kernel.kctl->ui32ReadOffset];
 assert(cmd.eCmdType==RGXFWIF_KCCB_CMD_KICK && cmd.eDM==3);
 assert(cmd.uCmdData.sCmdKickData.psContext.ui32Addr==a04_rgx_render_fw_address(view,view->tq));
 nextClient=cmd.uCmdData.sCmdKickData.ui32CWoffUpdate;assert(nextClient==296);
 auto header=(RGXFWIF_CCB_CMD_HEADER *)view->commands[3];
 assert(header->eCmdType==0x2abc80cc && header->ui32CmdSize==208);
 /* Independent scalar oracle of native producer bytes; remaining words zero. */
 unsigned char expected[208]={};
 const unsigned offsets[]={4,0x18,0x20,0x28,0x2c,0x60,0x80,0x88};
 const uint64_t values[]={0x1ffc,0x20002,0xa3000,0x202,0x20,3,0x0200003c68800000ULL,0x0000001f0000001fULL};
 for(unsigned i=0;i<8;i++)for(unsigned b=0;b<(i<5?4U:8U);b++)expected[offsets[i]+b]=(unsigned char)(values[i]>>(8*b));
 assert(!memcmp(view->commands[3]+40,expected,208));
 auto update=(RGXFWIF_CCB_CMD_HEADER *)(view->commands[3]+248);
 assert(update->eCmdType==A04_RGX_CLIENT_UPDATE && update->ui32CmdSize==8);
 auto ufo=(RGXFWIF_UFO *)(view->commands[3]+288);
 assert(ufo->puiAddrUFO.ui32Addr==view->sync_addr.ui32Addr && *view->sync==0);
 sequence=ufo->ui32Value;assert(sequence==1 && view->control[3]->ui32WriteOffset==0);
}
static void wait(void *,uint32_t us){
 assert(us==10);++waits;if(mode==3)return;
 if(kicks==2){
  if(mode==9)return; /* flush/SYNC unresponsive */
  if(mode==8)memset(view->bif.data.cpu,0,4096); /* only visible after SLC flush */
  *kernel.command_sync=1;kernel.kctl->ui32ReadOffset=3;return;
 }
 /* Simulated FW only: CPU driver must never manufacture these pixels. */
 if(mode==1){kernel.trace->ui32FWFaults=7;kernel.trace->ui32HWRStateFlags=0x100;}
 if(mode!=1 && mode!=8)memset(view->bif.data.cpu,0,4096);
 if(mode==2)((unsigned char *)view->bif.data.cpu)[4096]=0;
 *view->sync=sequence;
 view->control[3]->ui32ReadOffset=view->control[3]->ui32WriteOffset=view->control[3]->ui32DepOffset=nextClient;
 kernel.kctl->ui32ReadOffset=kernel.kctl->ui32WriteOffset;
}
static a04_rgx_command_io io={nullptr,publish,acquire,kick,wait};
static void clearCases(){
 for(mode=0;mode<10;mode++){
  attempts=0;kicks=waits=published=0;failAllocation=failPrepare=0;transport={};
  A04BR3RogueMemory source;A04BR3RogueRenderMemory owner;
  assert(owner.prepareCold(source,10,0xa800000000ULL,0xba00000000ULL));
  assert(owner.clearPrepared());view=owner.view();owner.pinForFirmware();
  assert(!a04_rgx_fw_runtime_init(&kernel,mainbuf,0xa0000000,configbuf,0xa1000000,26000000));
  kernel.init->bFirmwareStarted=IMG_TRUE;auto &c=kernel.init->sRGXCompChecks;
  c.bUpdated=IMG_TRUE;c.ui32DDKVersion=0x1000b;c.ui32DDKBuild=5425693;c.ui32FWProcessorVersion=1;
  c.sFWBVNC.ui64BVNC=c.sHWBVNC.ui64BVNC=0x001600280036001eULL;
  auto pds=(unsigned char *)owner.executionHeap(0)->data.cpu;
  assert(a04_rgx_fw_u32(pds+8)==(uint32_t)(view->bif.va>>2));
  assert(a04_rgx_fw_u32(pds+12)==((uint32_t)(view->bif.va>>34)|(1U<<19)));
  if(mode==4)((unsigned char *)owner.executionHeap(1)->data.cpu)[0]^=1;
  if(mode==5)pds[0]^=1;
  if(mode==6)memset(view->bif.data.cpu,0,4096); /* pre-zero is rejected */
  if(mode==7)kernel.init->sPDSExecBase.uiAddr+=4096;
  int result=owner.clear(kernel,io,transport);
  if(mode==0)assert(result==A04_CCB_OK && owner.clearedWords()==1024 && !owner.guardErrors());
  if(mode==1)assert(result==8 && !owner.clearedWords() && !owner.guardErrors() && owner.observation().valid && owner.observation().fwfaults==7 && owner.observation().hwr==0x100 && owner.readbackPixelsValid());
  if(mode==2)assert(result==8 && owner.clearedWords()==1024 && owner.guardErrors()==1);
  if(mode==3){assert(result==A04_CCB_TIMEOUT && waits==10000 && owner.observation().valid);
   auto &o=owner.observation();assert(o.kread==0 && o.kwrite==1 && o.cread==0 && !o.ufo && o.cexpected==296);}
  if(mode==8)assert(result==0 && owner.beforeA5()==1024 && !owner.afterA5() && owner.clearedWords()==1024);
  if(mode==9)assert(result==A04_CCB_TIMEOUT && transport.poisoned && owner.readbackObservation().valid && owner.readbackObservation().polls==10000 && !owner.clearedWords() && !owner.readbackPixelsValid());
  if(mode<4 || mode>=8){unsigned expectedKicks=mode==3?1:2;assert(kicks==expectedKicks);
   assert(owner.clear(kernel,io,transport)==A04_CCB_OFFLINE && kicks==expectedKicks);
   if(mode!=3 && mode!=9)assert(owner.readbackResult()==0);
   if(mode==1)assert(owner.beforeA5()==1024 && owner.afterA5()==1024 && owner.firstWord()==0xa5a5a5a5U);
  }else assert(result==A04_CCB_INPUT && !kicks && !published && !waits);
  owner.reset();assert(live==11);
  /* Test-only recovery after simulated power-off. Production quarantines. */
  for(unsigned n=0;n<total;n++)if(retained[n])retained[n]->release();assert(!live);
 }
}
int main(){
 for(unsigned failure=1;failure<=5;failure++) {
  attempts=0;failAllocation=failure;A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(!r.prepareCold(source,10) && !live && !sourceTable[0x80]);
 }
 failAllocation=0;
 for(unsigned failure=1;failure<=5;failure++) {
  attempts=0;failPrepare=failure;A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(!r.prepareCold(source,10) && !live && !sourceTable[0x80]);
 }
 failPrepare=0;attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(r.prepareCold(source,10) && live==5 && r.view());
  assert(sourceTable[0x80] && !sourceTable[0x90]);assert(a04_rgx_render_view_valid(r.view()));
  r.reset();assert(!sourceTable[0x80] && !live && !r.view());}
 attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRenderMemory r;sourceTable[0]|=4;
  assert(!r.prepareCold(source,10) && !live && !attempts);}
 for(unsigned fault=1;fault<=11;fault++) {
  attempts=0;failAllocation=fault;A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(!r.prepareCold(source,10,0xa800000000ULL,0xba00000000ULL) && !live && !sourceTable[0x80]);
 }
 failAllocation=0;
 for(unsigned fault=1;fault<=11;fault++) {
  attempts=0;failPrepare=fault;A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(!r.prepareCold(source,10,0xa800000000ULL,0xba00000000ULL) && !live && !sourceTable[0x80]);
 }
 failPrepare=0;attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(r.prepareCold(source,10,0xa800000000ULL,0xba00000000ULL) && live==11 && r.ownedBytes()==290816);
  auto general=r.view()->bif;auto pds=r.executionHeap(0);auto usc=r.executionHeap(1);
  assert(pds && usc && !r.executionHeap(2) && pds->pc.pa==general.pc.pa && usc->read_only);
  const a04_rgx_bif heaps[]={general,*pds,*usc};uint64_t physical=0;
  for(auto &h:heaps)for(unsigned n=0;n<65536;n++)
   assert(!a04_rgx_bif_translate(&h,h.va+n,&physical) && physical==h.data.pa+n);
  r.reset();assert(!live && !sourceTable[0x80]);}
 for(unsigned bad=0;bad<3;bad++) {
  attempts=0;A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  uint64_t pds=bad==0?0:0xa800000000ULL;
  uint64_t usc=bad==1?pds:(bad==2?A04_RGX_RENDER_SCRATCH_VA:0xba00000000ULL);
  assert(!r.prepareCold(source,10,pds,usc) && !live && !sourceTable[0x80]);
 }
 attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(r.prepareCold(source,10));sourceTable[0]|=4;
  r.reset();assert(live==5 && sourceTable[0x80]); /* forgot pin -> auto quarantine */
  assert(!r.prepareCold(source,10));}
 /* Harness reclaims only after mock power-off. Never done by production. */
 for(unsigned n=0;n<total;n++)if(retained[n])retained[n]->release();assert(!live);
 attempts=0;
 {A04BR3RogueMemory source;A04BR3RogueRenderMemory r;
  assert(r.prepareCold(source,10));r.pinForFirmware();r.reset();assert(live==5);}
 for(unsigned n=0;n<total;n++)if(retained[n])retained[n]->release();assert(!live && cleaned);
 clearCases();
}
''')
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                '-fsanitize=address,undefined','-I',str(p),'-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

    def test_arm64_original_owner_and_executor_compile(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'IOKit').mkdir()
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
using vm_offset_t=uintptr_t;using IOByteCount=uint64_t;extern void *kernel_task;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,
 kIOMapWriteCombineCache=0x400,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor {public:
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(void *,unsigned,uint64_t,uint64_t);
 int prepare();void complete();void release();void *getBytesNoCopy();
 uint64_t getPhysicalSegment(int,IOByteCount *);
};
extern "C" void flush_dcache(vm_offset_t,unsigned,int);
''')
            (p/'A04BR3RogueMemory.h').write_text('''#include <IOKit/IOBufferMemoryDescriptor.h>
class A04BR3RogueMemory {public:bool stagedMemory(void **,uint64_t *);};
''')
            # Preserve ALL production ARM instructions; copy only to select stub Memory.
            (p/'A04BR3RogueRenderMemory.h').write_text((GPU/'A04BR3RogueRenderMemory.h').read_text())
            (p/'target.cpp').write_text('''#include "A04BR3RogueRenderMemory.h"
bool prepare(A04BR3RogueRenderMemory *r,A04BR3RogueMemory *s){return r->prepareCold(*s,1);}
void reset(A04BR3RogueRenderMemory *r){r->reset();}
int submit(A04BR3RogueRenderMemory *r,a04_rgx_fw_runtime *k,a04_rgx_command_io *io){return r->update(*k,*io,3);}
int clear(A04BR3RogueRenderMemory *r,a04_rgx_fw_runtime *k,a04_rgx_command_io *io){a04_rgx_command_state s={};return r->clear(*k,*io,s);}
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                '-fno-exceptions','-fno-rtti','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=8192',
                '-I',str(p),'-I',str(GPU),'-c',str(p/'target.cpp'),'-o',str(p/'target.o')],check=True)

if __name__=='__main__':unittest.main()
