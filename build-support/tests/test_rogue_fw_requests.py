# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual bounded FWCCB reset snapshot, not a firmware emulator or ACK path."""
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
CODE=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueFWRequests.h"
alignas(8) static uint8_t ring[1280],saved[1280];
static void put(uint8_t *p,uint64_t value,unsigned n){for(unsigned b=0;b<n;b++)p[b]=(uint8_t)(value>>(8*b));}
int main(){
 for(unsigned i=0;i<32;i++){
  auto p=ring+40*i;put(p,0x2abc0069,4);put(p+4,i+100,4);put(p+8,i+10,4);put(p+12,i%6,4);
  put(p+16,i+200,4);put(p+20,i%2,4);put(p+24,0x123456780000ULL+i*4096,8);put(p+32,0xa800000000ULL+i*4096,8);
 }
 memcpy(saved,ring,1280);
 for(unsigned read=0;read<32;read++)for(unsigned pending=0;pending<32;pending++){
  a04_rgx_fw_requests out={};assert(!a04_rgx_fw_requests_snapshot(&out,sizeof(out),ring,sizeof(ring),read,(read+pending)&31));
  assert(out.valid && out.pending==pending && out.count==(pending<2?pending:2));
  for(unsigned n=0;n<out.count;n++){
   unsigned i=(read+n)&31;const auto &v=out.first[n];
   assert(v.reset && v.slot==i && v.type==0x2abc0069 && v.flags==i+100 && v.context==i+10 && v.reason==i%6 && v.job==i+200 && v.pageFault==i%2);
   assert(v.pc==0x123456780000ULL+i*4096 && v.fault==0xa800000000ULL+i*4096);
  }
  assert(!memcmp(saved,ring,sizeof(ring))); // no consume, including31->0
 }
 a04_rgx_fw_requests out,before;memset(&out,0xa5,sizeof(out));memcpy(&before,&out,sizeof(out));
 for(unsigned bytes=0;bytes<1280;bytes++){assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out),ring,bytes,0,2));assert(!memcmp(&out,&before,sizeof(out)));}
 assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out)-1,ring,1280,0,2));
 assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out),ring,1280,32,0));
 assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out),ring,1280,0,32));
 assert(a04_rgx_fw_requests_snapshot(nullptr,sizeof(out),ring,1280,0,1));
 assert(a04_rgx_fw_requests_snapshot((a04_rgx_fw_requests *)(UINTPTR_MAX-7),sizeof(out),ring,1280,0,1));
 assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out),nullptr,1280,0,1));
 assert(a04_rgx_fw_requests_snapshot(&out,sizeof(out),(void *)(UINTPTR_MAX-100),1280,0,1));
 assert(a04_rgx_fw_requests_snapshot((a04_rgx_fw_requests *)ring,sizeof(out),ring,1280,0,1));
 assert(!memcmp(&out,&before,sizeof(out)) && !memcmp(saved,ring,sizeof(ring)));
 put(ring,0xdeadbeef,4);assert(!a04_rgx_fw_requests_snapshot(&out,sizeof(out),ring,1280,0,1));
 assert(out.count==1 && !out.first[0].reset && out.first[0].type==0xdeadbeef && !out.first[0].context);
}
'''
class FWRequests(unittest.TestCase):
 def test_actual_ring_bounds_wrap_no_consume_and_native_reset_fields(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.cpp').write_text(CODE)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
 def test_arm64_actual_parser(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'call.cpp').write_text('#include "A04BR3RogueFWRequests.h"\nint call(a04_rgx_fw_requests *o,const void *r,unsigned n,unsigned read,unsigned write){return a04_rgx_fw_requests_snapshot(o,sizeof(*o),r,n,read,write);}\n')
   subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'call.cpp'),'-o',str(p/'call.o')],check=True)
if __name__=='__main__':unittest.main()
