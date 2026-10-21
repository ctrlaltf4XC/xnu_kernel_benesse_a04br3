# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual14-lease cold-stage dispatch, rollback and source image validation.
No live registers/submission/full208/USCasset/pixel execution model.
"""
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
CODE=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueNativeResources.h"
alignas(4096) static uint8_t pc[4096],pd[4][4096],pt[4][4096],data[4][65536],bank[65536];
static uint8_t bankBefore[65536],pcBefore[4096],pdBefore[sizeof(pd)],ptBefore[sizeof(pt)];
static a04_rgx_bif heaps[4];static a04_rgx_render_resources r;
static unsigned holds,drops,cleaned,acquired,refs,failHold,failClean,failAcquire;
static a04_rgx_render_buffer all[14];
static bool same(const a04_rgx_render_buffer &a,const a04_rgx_render_buffer &b){return a.cpu==b.cpu&&a.pa==b.pa&&a.bytes==b.bytes;}
static int hold(void *,const a04_rgx_render_buffer *b){assert(holds<14&&same(*b,all[holds]));++holds;if(holds==failHold)return 1;++refs;return 0;}
static void drop(void *,const a04_rgx_render_buffer *b){assert(refs&&same(*b,all[refs-1]));--refs;++drops;}
static int clean(void *,const a04_rgx_render_buffer *b){
 assert(refs==14&&holds==14&&same(*b,all[cleaned]));++cleaned;
 // Every image write preceded even the FIRST cache callback, never target
 // CPU synthesis. Sampler36 island survives the A5 target/guard initialization.
 assert(data[0][0]==0xa5&&data[0][8192]!=0xa5);
 assert(a04_rgx_fw_u32(data[1]+192)==96&&a04_rgx_fw_u32(data[1]+264)==0xf0000009);
 assert(a04_rgx_fw_u32(data[3]+128+56)==192);
 return cleaned==failClean;
}
static int acquire(void *){++acquired;return failAcquire;}
static a04_rgx_native_resource_io io={nullptr,hold,drop,clean,acquire};
static void setup(){
 const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL,0x10000};
 for(unsigned i=0;i<4;i++)heaps[i]={{pc,0x80000000,4096},{pd[i],0x80010000+i*8192,4096},
  {pt[i],0x80011000+i*8192,4096},{data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 a04_rgx_render_buffer b={bank,0x82000000,65536};assert(!a04_rgx_render_build(&r,&b,&heaps[0],1));
 assert(!a04_rgx_heap_map_build(heaps,4));memset(data,0x3c,sizeof(data));
 memcpy(bankBefore,bank,sizeof(bank));memcpy(pcBefore,pc,sizeof(pc));memcpy(pdBefore,pd,sizeof(pd));memcpy(ptBefore,pt,sizeof(pt));
 all[0]=r.bank;all[1]=heaps[0].pc;for(unsigned n=0;n<4;n++){all[2+3*n]=heaps[n].pd;all[3+3*n]=heaps[n].pt;all[4+3*n]=heaps[n].data;}
 holds=drops=cleaned=acquired=refs=failHold=failClean=failAcquire=0;
}
static void immutableBankTables(){assert(!memcmp(bank,bankBefore,sizeof(bank))&&!memcmp(pc,pcBefore,sizeof(pc))&&!memcmp(pd,pdBefore,sizeof(pd))&&!memcmp(pt,ptBefore,sizeof(pt)));}
static int stage(A04BR3RogueNativeResources &o){return o.stage(&r,sizeof(r),heaps,sizeof(heaps),io);}
static const a04_rgx_native_resource_image *pin(A04BR3RogueNativeResources &o){return o.pinImages(&r,sizeof(r),heaps,sizeof(heaps));}
int main(){
 setup();a04_rgx_native_resource_image out,saved;memset(&out,0x6c,sizeof(out));saved=out;
 assert(a04_rgx_native_resource_build(&out,sizeof(out)-1,&r,sizeof(r),heaps,sizeof(heaps))&&!memcmp(&out,&saved,sizeof(out)));
 for(unsigned n=0;n<4;n++)assert(a04_rgx_native_resource_build(&out,sizeof(out),&r,sizeof(r),heaps,n*sizeof(heaps[0]))&&!memcmp(&out,&saved,sizeof(out)));
 assert(a04_rgx_native_resource_build(&out,sizeof(out),&r,sizeof(r)-1,heaps,sizeof(heaps))&&!memcmp(&out,&saved,sizeof(out)));
 assert(a04_rgx_native_resource_build((a04_rgx_native_resource_image *)data[0],sizeof(out),&r,sizeof(r),heaps,sizeof(heaps)));
 auto rBad=r;rBad.bank.pa=heaps[3].data.pa; // bank-vs-ISP physical alias
 assert(a04_rgx_native_resource_build(&out,sizeof(out),&rBad,sizeof(rBad),heaps,sizeof(heaps))&&!memcmp(&out,&saved,sizeof(out)));
 for(unsigned n=0;n<4;n++){
  auto table=(uint64_t *)pt[n];unsigned at=(heaps[n].va>>12)&511;uint64_t old=table[at];table[at]=0;
  assert(a04_rgx_native_resource_build(&out,sizeof(out),&r,sizeof(r),heaps,sizeof(heaps))&&!memcmp(&out,&saved,sizeof(out)));table[at]=old;
 }
 // Read-only USC permissions are part of the actual page walk, not merely
 // the metadata bool. A writable-code PTE must not pass through preparation.
 {auto table=(uint64_t *)pt[2];unsigned at=(heaps[2].va>>12)&511;table[at]^=2;
 assert(a04_rgx_native_resource_build(&out,sizeof(out),&r,sizeof(r),heaps,sizeof(heaps))&&!memcmp(&out,&saved,sizeof(out)));table[at]^=2;}
 for(unsigned callback=0;callback<4;callback++){
  setup();A04BR3RogueNativeResources o;auto missing=io;
  if(callback==0)missing.hold=nullptr;if(callback==1)missing.drop=nullptr;
  if(callback==2)missing.clean=nullptr;if(callback==3)missing.acquire=nullptr;
  assert(o.stage(&r,sizeof(r),heaps,sizeof(heaps),missing)==1&&!holds&&!cleaned&&!refs);
  for(auto &d:data)for(auto byte:d)assert(byte==0x3c);immutableBankTables();
 }
 for(unsigned bad=1;bad<=14;bad++){
  setup();failHold=bad;A04BR3RogueNativeResources o;
  assert(stage(o)==2&&!o.prepared()&&!pin(o)&&holds==bad&&drops==bad-1&&!refs&&!cleaned);
  for(auto &d:data)for(auto byte:d)assert(byte==0x3c);immutableBankTables();assert(o.resetCold());
 }
 for(unsigned bad=1;bad<=14;bad++){
  setup();failClean=bad;A04BR3RogueNativeResources o;
  assert(stage(o)==3&&!o.prepared()&&!pin(o)&&holds==14&&drops==14&&!refs&&cleaned==bad);
  immutableBankTables();assert(o.resetCold());
 }
 setup();{
  A04BR3RogueNativeResources o;r.control[3]->ui32WriteOffset=8;assert(stage(o)&&!holds);r.control[3]->ui32WriteOffset=0;
  assert(!stage(o)&&o.prepared()&&refs==14&&cleaned==14);assert(stage(o));
  immutableBankTables();
  assert(!a04_rgx_native_resource_build(&out,sizeof(out),&r,sizeof(r),heaps,sizeof(heaps)));
  assert(!memcmp(data[0]+8192,out.background.shared,36));
  for(unsigned n=0;n<65536;n++)if(n<8192||n>=8228)assert(data[0][n]==0xa5);
  assert(!memcmp(data[1],out.pds,sizeof(out.pds))&&!memcmp(data[2],out.usc,sizeof(out.usc)));
  assert(!memcmp(data[3],out.isp.control,128)&&!memcmp(data[3]+128,out.isp.quad,88));
  const unsigned regions[]={0,0,0,1,1,1,2,2,3,3,3};
  const unsigned offsets[]={0,4096,8192,64,192,256,96,104,0,128,216};
  for(unsigned n=0;n<sizeof(regions)/sizeof(regions[0]);n++){
   data[regions[n]][offsets[n]]^=1;assert(!pin(o)&&o.prepared()&&refs==14);data[regions[n]][offsets[n]]^=1;
  }
  auto oldPA=heaps[3].data.pa;heaps[3].data.pa+=4096;assert(!pin(o));heaps[3].data.pa=oldPA;
  failAcquire=1;assert(!pin(o)&&o.prepared()&&refs==14);failAcquire=0;
  *r.sync=1;assert(!pin(o));*r.sync=0;r.control[3]->ui32WriteOffset=8;assert(!pin(o));r.control[3]->ui32WriteOffset=0;
  assert(o.resetCold()&&!refs&&drops==14);immutableBankTables();
 }
 setup();{
  A04BR3RogueNativeResources o;assert(!stage(o));
  const auto *images=pin(o);assert(images&&o.pinned()&&refs==14&&!drops);
  assert(images->isp.fragment_pds_reference.va==heaps[1].va+192&&images->isp.fragment_usc_reference.va==heaps[2].va+96);
  assert(images->background.target_reference.va==images->isp.target_reference.va);
  assert(images->background.shared_destination.va==heaps[0].va+8192);
  assert(!o.resetCold()&&!pin(o)&&stage(o));immutableBankTables();
 }
 assert(refs==14&&!drops); // destructor quarantines all firmware-mapping leases
 puts("PASS14 retain +14 cache failures, guards/shared island, actual CPU image staging/pin; no full208 or pixel simulation");
}
'''
class NativeResources(unittest.TestCase):
    def test_real_fourheap_stage_failure_lifetimes_and_integrity(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.cpp').write_text(CODE)
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),str(p/'case.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'arm.cpp').write_text('''#include "A04BR3RogueNativeResources.h"
int stage(A04BR3RogueNativeResources *o,const a04_rgx_render_resources *r,const a04_rgx_bif *h,const a04_rgx_native_resource_io *io){return o->stage(r,sizeof(*r),h,4*sizeof(*h),*io);}
const a04_rgx_native_resource_image *pin(A04BR3RogueNativeResources *o,const a04_rgx_render_resources *r,const a04_rgx_bif *h){return o->pinImages(r,sizeof(*r),h,4*sizeof(*h));}
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)
if __name__=='__main__':unittest.main()
