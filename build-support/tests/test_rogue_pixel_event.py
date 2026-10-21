# SPDX-License-Identifier: MIT
"""Compare production PDS CPU output with pinned original Mesa producer.

No GPU/firmware/clear emulation: USC ISA generation and real owned-buffer
render completion remain separate prerequisites to a hardware clear test.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
FIXTURE=HERE/'fixtures/rogue_pixel_event'

class RoguePixelEvent(unittest.TestCase):
    def test_pinned_fixture_integrity(self):
        meta=json.loads((FIXTURE/'provenance.json').read_text())
        self.assertEqual(meta['revision'],'e1ef8f046c6e37ec1e2a13229560fbb824e5118f')
        self.assertEqual(hashlib.sha256((FIXTURE/'mesa_pixel_event.h').read_bytes()).hexdigest(),meta['fixture_sha256'])

    def test_exact_source_comparison_old_ddk_padding_and_rejections(self):
        source=r'''
#include <string.h>
#include "A04BR3RoguePixelEvent.h"
#include "mesa_pixel_event.h"
static void compare(uint32_t offset,unsigned temps,uint32_t lo,uint32_t hi){
 a04_rgx_pixel_event got;memset(&got,0xcc,sizeof(got));
 assert(!a04_rgx_pixel_event_build(&got,offset,temps,lo,hi));
 uint32_t state[]={lo,hi},data[8]={},code[8]={};
 pvr_device_info device={false};pvr_pds_event_program p={};
 p.emit_words=state;p.num_emit_word_pairs=1;
 pvr_pds_setup_doutu(&p.task_control,offset,temps,0,false);
 pvr_pds_generate_pixel_event(&p,nullptr,PDS_GENERATE_SIZES,&device);
 assert(p.data_size==5 && p.code_size==2);
 assert(pvr_pds_generate_pixel_event(&p,data,PDS_GENERATE_DATA_SEGMENT,&device)==data+5);
 assert(pvr_pds_generate_pixel_event(&p,code,PDS_GENERATE_CODE_SEGMENT,&device)==code+2);
 assert(!memcmp(got.image,data,20));assert(!memcmp(got.image+32,code,8));
 // Native board FUN_0013c634 reserves 2*n+3 code DWORDs, NOT Mesa n+1.
 assert(A04_RGX_PIXEL_EVENT_DATA_WORDS==5 && A04_RGX_PIXEL_EVENT_CODE_WORDS==5);
 assert(A04_RGX_PIXEL_EVENT_CODE_WRITTEN_WORDS==2 && A04_RGX_PIXEL_EVENT_USED_BYTES==52);
 for(unsigned i=20;i<32;i++)assert(!got.image[i]);
 for(unsigned i=40;i<64;i++)assert(!got.image[i]);
 // Source-decoded instruction fields, not opaque shader assets.
 assert(code[0]==0xf0040101 && code[1]==0xf4000002 && data[4]==0x90000002);
 assert((code[0]&7)==1 && ((code[0]>>8)&127)==1 && ((code[0]>>16)&255)==4);
 assert((code[1]&(1u<<26)) && (code[1]&7)==2);
}
int main(){
 static_assert(sizeof(a04_rgx_pixel_event)==64,"bounded CPU-owned result");
 uint32_t rng=0x12345678;
 for(unsigned i=0;i<4096;i++){
  rng=rng*1664525u+1013904223u;uint32_t lo=rng;
  rng=rng*1664525u+1013904223u;uint32_t hi=rng;
  compare(rng&~3u,i%253,lo,hi);
 }
 for(unsigned temps=0;temps<=252;temps++){
  compare(0,temps,0,0);compare(0xfffffffc,temps,0xffffffff,0xffffffff);
 }
 a04_rgx_pixel_event saved,bad;memset(&saved,0xa5,sizeof(saved));bad=saved;
 assert(a04_rgx_pixel_event_build(nullptr,0,0,0,0));
 for(unsigned align=1;align<4;align++){
  assert(a04_rgx_pixel_event_build(&bad,align,0,0,0));assert(!memcmp(&bad,&saved,sizeof(bad)));
 }
 assert(a04_rgx_pixel_event_build(&bad,UINT64_C(0x100000000),0,0,0));
 assert(!memcmp(&bad,&saved,sizeof(bad)));
 for(unsigned temps: {253u,254u,255u,0xffffffffu}){
  assert(a04_rgx_pixel_event_build(&bad,0,temps,0,0));assert(!memcmp(&bad,&saved,sizeof(bad)));
 }
}
'''
        source='#include <initializer_list>\n'+source
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-I',str(GPU),'-I',str(FIXTURE),
                            str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

if __name__=='__main__':unittest.main()
