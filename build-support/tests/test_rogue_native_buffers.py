# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual kernel allocator adapter with modeled IOBuffer API + ARM64 compile.
Allocator/cache/lease dispatch is executed; no FW mapping/MMIO/GPU execution.
"""
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
STUB=r'''
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
using IOByteCount=uint64_t;using vm_offset_t=uintptr_t;
using task_t=void *;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,kIOMapWriteCombineCache=0x400,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor;
static IOBufferMemoryDescriptor *allocated[16];
static unsigned live,attempts,prepCalls,completeCalls,releases,retains,cleaned;
static unsigned failAlloc,failPrepare,badLength,badPA,aliasPA,badAfterHolds;
class IOBufferMemoryDescriptor {
public:void *cpu;uint64_t pa,bytes;unsigned refs=1,slot;bool isPrepared=false;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(task_t,unsigned options,uint64_t size,uint64_t mask){
  unsigned at=++attempts;assert(at<=14&&mask==0xfffff000);
  assert((options&(kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMemoryMapperNone))==(kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMemoryMapperNone));
  assert(((options&kIOMapWriteCombineCache)!=0)==(size==65536));
  if(at==failAlloc)return nullptr;
  auto b=new IOBufferMemoryDescriptor;b->bytes=size;b->slot=at;
  b->pa=0x90000000ULL+(at-1)*0x20000;
  if(at==badPA)b->pa=0x100000000ULL;if(at==aliasPA)b->pa=0x90000000ULL;
  assert(!posix_memalign(&b->cpu,4096,size));memset(b->cpu,0xcc,size);allocated[at]=b;++live;return b;
 }
 int prepare(){++prepCalls;if(slot==failPrepare)return 1;assert(!isPrepared);isPrepared=true;return 0;}
 void complete(){assert(isPrepared);isPrepared=false;++completeCalls;}
 void retain(){assert(isPrepared&&refs);++refs;++retains;}
 void release(){assert(refs);++releases;if(--refs)return;assert(!isPrepared);free(cpu);allocated[slot]=nullptr;--live;delete this;}
 void *getBytesNoCopy(){return cpu;}
 uint64_t getPhysicalSegment(unsigned,IOByteCount *length){*length=bytes-((slot==badLength)||(badAfterHolds&&retains==14&&slot==1));return pa;}
};
static void flush_dcache(vm_offset_t cpu,unsigned length,int flags){
 assert(!flags&&live==14&&retains==14);bool found=false;
 for(unsigned i=1;i<=14;i++)if(allocated[i]->cpu==(void *)cpu){assert(length==allocated[i]->bytes&&allocated[i]->refs==2&&allocated[i]->isPrepared);found=true;}
 assert(found);++cleaned;
}
'''
CODE=r'''
#include <stdio.h>
#include "A04BR3RogueNativeBuffers.h"
static void setup(){assert(!live);memset(allocated,0,sizeof(allocated));attempts=prepCalls=completeCalls=releases=retains=cleaned=0;failAlloc=failPrepare=badLength=badPA=aliasPA=badAfterHolds=0;}
int main(){
 for(unsigned kind=0;kind<6;kind++){
  setup();parsePresent=kind!=0;parseValue=kind==2?0:kind==3?2:1;
  if(kind==5)failPrepare=7;
  TrialDriver driver;driver.run(kind==4?0:0x001600280036001eULL);
  assert(!live&&!driver.nativeTrial.pinned());
  if(kind<1||kind==2||kind==3)assert(!attempts&&!driver.properties&&!driver.logs);
  else if(kind==4)assert(!attempts&&driver.result==3&&driver.logs==1);
  else if(kind==5)assert(attempts==7&&driver.result==2&&driver.logs==1);
  else assert(attempts==14&&driver.result==1&&driver.logs==1&&completeCalls==14&&releases==28);
 }
 setup();{
  A04BR3RogueNativeBuffers b;
  assert(b.coldTrial(0,0)==0&&b.coldTrial(2,0)==0&&!attempts);
  assert(b.coldTrial(1,0)==3&&!attempts);
  assert(b.coldTrial(1,0x001600280036001eULL)==1&&!live&&!b.view()&&!b.pinned());
  assert(attempts==14&&retains==14&&cleaned==14&&completeCalls==14&&releases==28);
 }
 for(unsigned fail=1;fail<=14;fail++){
  setup();failAlloc=fail;A04BR3RogueNativeBuffers b;
  assert(b.coldTrial(1,0x001600280036001eULL)==2&&!live&&!b.view()&&!b.pinned());
 }
 setup();{
  A04BR3RogueNativeBuffers b;assert(b.prepareCold(1,0));
  assert(b.coldTrial(1,0x001600280036001eULL)==3&&live==14);
  assert(b.verifyCold()&&!b.pinned());
  ((uint8_t *)b.heapViews()[0].data.cpu)[8192]^=1;assert(!b.verifyCold());
  assert(b.resetCold()&&!live);
 }

 for(unsigned fail=1;fail<=14;fail++){
  setup();failAlloc=fail;A04BR3RogueNativeBuffers b;
  assert(!b.prepareCold(1,0x10000)&&!b.view()&&!b.ownedBytes()&&!live&&!retains&&!cleaned);
  assert(attempts==fail&&prepCalls==fail-1&&completeCalls==fail-1&&releases==fail-1);
 }
 for(unsigned fail=1;fail<=14;fail++){
  setup();failPrepare=fail;A04BR3RogueNativeBuffers b;
  assert(!b.prepareCold(1,0x10000)&&!b.view()&&!live&&!retains&&!cleaned);
  assert(attempts==fail&&prepCalls==fail&&completeCalls==fail-1&&releases==fail);
 }
 for(unsigned kind=0;kind<3;kind++){
  setup();if(kind==0)badLength=5;if(kind==1)badPA=5;if(kind==2)aliasPA=5;
  A04BR3RogueNativeBuffers b;assert(!b.prepareCold(1,0x10000)&&!live&&!retains);
  assert(attempts==5&&completeCalls==5&&releases==5);
 }
 setup();{
  badAfterHolds=1;A04BR3RogueNativeBuffers b;
  assert(!b.prepareCold(1,0x10000)&&!live&&retains==14&&completeCalls==14&&releases==28&&!cleaned);
 }
 setup();{
  A04BR3RogueNativeBuffers b;assert(b.prepareCold(1,0x10000));badLength=14;
  assert(!b.pinImages()&&b.resetCold()&&!live&&completeCalls==14&&releases==28);
 }
 setup();{
  A04BR3RogueNativeBuffers b;
  assert(!b.prepareCold(0,0x10000)&&!attempts);
  assert(!b.prepareCold(1,1)&&!attempts);
  assert(!b.prepareCold(1,0x40000000)&&!attempts);
  assert(!b.prepareCold(0xffffffffU,0x10000)&&!attempts);
  assert(b.prepareCold(7,0x10000)&&live==14&&retains==14&&cleaned==14&&b.ownedBytes()==364544);
  auto r=b.view();auto h=b.heapViews();assert(r&&h&&!b.pinned());
  assert(h[3].va==0x10000&&h[2].read_only);
  auto general=(uint8_t *)h[0].data.cpu;auto pds=(uint8_t *)h[1].data.cpu;auto isp=(uint8_t *)h[3].data.cpu;
  assert(a04_rgx_fw_u32(pds+192)==96&&a04_rgx_fw_u32(isp+128+56)==192);
  for(unsigned i=0;i<4096;i++)assert(general[i]==0xa5);
  assert(a04_rgx_fw_u32(general+8192)==0&&a04_rgx_fw_u32(general+8196)==0x2400);
  uint64_t physical=allocated[14]->pa;allocated[14]->pa+=4096;assert(!b.pinImages());allocated[14]->pa=physical;
  general[8192]^=1;assert(!b.pinImages());general[8192]^=1;
  assert(b.resetCold()&&!b.view()&&!live&&completeCalls==14&&releases==28);
 }
 setup();{
  A04BR3RogueNativeBuffers b;assert(b.prepareCold(1,0)&&b.ownedBytes()==364544);
  assert(b.pinImages()&&b.pinned());
  assert(!b.resetCold()&&!b.prepareCold(2,0x10000));
 }
 assert(live==14&&!completeCalls&&!releases); // source destructor did not tear down live-capable pages
 for(unsigned i=1;i<=14;i++)assert(allocated[i]->refs==2&&allocated[i]->isPrepared);
 // Test-only reclaim after verifying the quarantine. This is NOT a production
 // teardown API, FW acknowledgement, device reset or physical operation.
 for(unsigned i=1;i<=14;i++){auto p=allocated[i];p->complete();p->release();p->release();}
 assert(!live);
 puts("PASS14 allocation +14 prepare failures, segment/mask/alias rejection, real adapter retain/cache dispatch and quarantine; no FW mapping");
}
'''
DECL=r'''
#pragma once
#include <stdint.h>
using IOByteCount=uint64_t;using vm_offset_t=uintptr_t;
using task_t=void *;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,kIOMapWriteCombineCache=0x400,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor {public:
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(task_t,unsigned,uint64_t,uint64_t);
 int prepare();void complete();void retain();void release();void *getBytesNoCopy();
 uint64_t getPhysicalSegment(unsigned,IOByteCount *);
};
void flush_dcache(vm_offset_t,unsigned,int);
'''
class NativeBuffers(unittest.TestCase):
    def test_allocator_adapter_failures_cache_and_actual_arm64_caller(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'IOKit').mkdir();(p/'arm').mkdir();(p/'kern').mkdir()
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text(STUB)
            (p/'kern/task.h').write_text('#pragma once\nstatic task_t kernel_task;\n')
            (p/'arm/caches_internal.h').write_text('#pragma once\n')
            driver=(GPU/'A04BR3RogueGPU.cpp').read_text()
            begin=driver.index('    unsigned nativeTrialEnabled=0;')
            end=driver.index('    /* This source-built display',begin)
            trial=driver[begin:end]
            prefix=r'''
static bool parsePresent;static unsigned parseValue;
static bool PE_parse_boot_argn(const char *name,unsigned *value,unsigned bytes){
 assert(!strcmp(name,"a04br3.gpu_trial")&&bytes==sizeof(unsigned));
 if(parsePresent)*value=parseValue;return parsePresent;
}
struct TrialDriver {
 A04BR3RogueNativeBuffers nativeTrial;unsigned properties=0,logs=0,result=99;
 void setProperty(const char *key,uint64_t value,unsigned bits){assert(!strcmp(key,"NativeResourceCPUTrialResult")&&bits==32);++properties;result=value;}
 void IOLog(const char *,unsigned value,unsigned bytes){++logs;assert(value==result&&bytes==(result==1?364544U:0U));}
 void run(uint64_t bvnc){
'''+trial+'\n}\n};\n'
            (p/'case.cpp').write_text(CODE.replace('static void setup()',prefix+'\nstatic void setup()',1))
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(p),'-I',str(GPU),str(p/'case.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text(DECL)
            (p/'kern/task.h').write_text('#pragma once\nextern task_t kernel_task;\n')
            (p/'caller.cpp').write_text('''#include "A04BR3RogueNativeBuffers.h"
bool prepare(A04BR3RogueNativeBuffers *b){return b->prepareCold(1,0x10000);}
const a04_rgx_native_resource_image *pin(A04BR3RogueNativeBuffers *b){return b->pinImages();}
bool reset(A04BR3RogueNativeBuffers *b){return b->resetCold();}
unsigned trial(A04BR3RogueNativeBuffers *b,unsigned enabled,uint64_t bvnc){return b->coldTrial(enabled,bvnc);}
const a04_rgx_native_resource_image *verify(A04BR3RogueNativeBuffers *b){return b->verifyCold();}
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wframe-larger-than=4096','-I',str(p),'-I',str(GPU),'-c',str(p/'caller.cpp'),'-o',str(p/'arm.o')],check=True)
if __name__=='__main__':unittest.main()
