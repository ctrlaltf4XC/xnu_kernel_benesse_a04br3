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
static IOBufferMemoryDescriptor *retained[256];static unsigned live,total,attempts,cleaned;
static unsigned failAllocation,failPrepare;
class IOBufferMemoryDescriptor {
public:void *cpu;uint64_t bytes,pa;unsigned id,slot;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(int,unsigned options,uint64_t size,uint64_t mask){
  assert(mask==0xfffff000);unsigned at=++attempts;if(at==failAllocation)return nullptr;
  assert(((options&kIOMapWriteCombineCache)!=0)==(size==65536));
  auto b=new IOBufferMemoryDescriptor;b->id=total++;b->slot=at;assert(b->id<256);
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
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                '-fno-exceptions','-fno-rtti','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=8192',
                '-I',str(p),'-I',str(GPU),'-c',str(p/'target.cpp'),'-o',str(p/'target.o')],check=True)

if __name__=='__main__':unittest.main()
