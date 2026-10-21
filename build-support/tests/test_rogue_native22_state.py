# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Real native22 state producer; no texture USC asset or accepted task."""
from pathlib import Path
import json,os,subprocess,tempfile,unittest
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
PDS=HERE/'fixtures/rogue_pixel_event'
CODE=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueNative22State.h"
#include "mesa_pixel_event.h"
alignas(4096) static uint8_t pc[4096],pd[4][4096],pt[4][4096],data[4][65536];
static a04_rgx_native22_state_input in;
static uint64_t rd(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
static void setup(){
 const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL,0x10000};
 in={};for(unsigned i=0;i<4;i++)in.heaps[i]={{pc,0x80000000,4096},
  {pd[i],0x80010000+i*8192,4096},{pt[i],0x80011000+i*8192,4096},
  {data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 assert(!a04_rgx_heap_map_build(in.heaps,4));memset(data,0xa5,sizeof(data));
 in.target={va[0],4096};in.shared={va[0]+8192,36};in.pds_data={va[1]+256,16};in.pds_code={va[1]+272,16};
}
static void rejects(const a04_rgx_native22_state_input &bad){
 a04_rgx_native22_state_image out,saved;memset(&out,0x6c,sizeof(out));saved=out;
 assert(a04_rgx_native22_state_build(&out,&bad)&&!memcmp(&out,&saved,sizeof(out)));
}
int main(){
 setup();unsigned cases=0;
 for(unsigned at=0;at<=61440;at+=128){
  in.target.va=in.heaps[0].va+at;in.shared.va=in.heaps[0].va+(at?0:8192);
  a04_rgx_native22_state_image out;assert(!a04_rgx_native22_state_build(&out,&in));++cases;
  assert(rd(out.shared)==UINT64_C(0x240000000000)&&!rd(out.shared+8));
  // Original13eeac expression, with native row26 field values and the exact
  // typed local surface descriptor12d28c. General format flags all0.
  uint32_t d[14]={0x22,0,0,32,32,0,0,1,1,1,32,0,(uint32_t)in.target.va,(uint32_t)(in.target.va>>32)};
  uint64_t image0=UINT64_C(12)<<27|UINT64_C(0)<<14|UINT64_C(1)<<11|UINT64_C(2)<<8|UINT64_C(3)<<5;
  image0|=(uint64_t)(d[3]-1)<<34;image0|=(uint64_t)(d[4]-1)<<48;image0|=4;
  uint64_t address=(uint64_t)d[12]|((uint64_t)d[13]<<32);
  uint64_t image1=(uint64_t)(d[10]-1)|((address&UINT64_C(0x3fffffffffffc))<<14)|(uint64_t)d[7]<<60;
  if(((0x0082u>>2)^0xffffu)&1)image1|=UINT64_C(0x800000000000000);
  assert(rd(out.shared+16)==image0&&rd(out.shared+24)==image1);
  assert(a04_rgx_fw_u32(out.shared+32)==255);
  assert(rd(out.pds_data)==in.shared.va&&a04_rgx_fw_u32(out.pds_data+8)==0xf0000009&&!a04_rgx_fw_u32(out.pds_data+12));
  // Pinned original Mesa DOUT encoder independently validates native DOUTD
  // dest0/const64[0]/controlconst2/END, not an invented shader word.
  assert(a04_rgx_fw_u32(out.pds_code)==0xc1400002);
  assert(a04_rgx_fw_u32(out.pds_code+4)==pvr_pds_inst_encode_dout(0,1,2,0,0));
  assert(a04_rgx_fw_u32(out.pds_code+8)==0xd3000000&&!a04_rgx_fw_u32(out.pds_code+12));
  assert(out.shared_destination.va==in.shared.va&&out.target_reference.va==in.target.va);
  assert(out.data_destination.va==in.pds_data.va&&out.code_destination.va==in.pds_code.va);
 }
 setup();
 auto bad=in;bad.shared=in.target;rejects(bad);
 bad=in;bad.pds_code=in.pds_data;rejects(bad);
 bad=in;bad.target.bytes=4095;rejects(bad);
 bad=in;bad.shared.bytes=35;rejects(bad);
 bad=in;bad.pds_data.bytes=12;rejects(bad);
 bad=in;bad.pds_code.bytes=12;rejects(bad);
 bad=in;bad.target.va+=1;rejects(bad);
 bad=in;bad.shared.va=UINT64_MAX;rejects(bad);
 bad=in;bad.pds_code.va=bad.heaps[1].va+65532;rejects(bad);
 bad=in;bad.heaps[2].read_only=false;rejects(bad);
 bad=in;bad.heaps[3].data.pa=bad.heaps[0].data.pa;rejects(bad);
 for(unsigned region=0;region<4;region++){
  auto table=(uint64_t *)pt[region];unsigned at=(in.heaps[region].va>>12)&511;
  uint64_t save=table[at];table[at]=0;rejects(in);table[at]=save;
 }
 a04_rgx_native22_state_image out,saved;memset(&out,0x6c,sizeof(out));saved=out;
 assert(a04_rgx_native22_state_build(nullptr,&in));
 assert(a04_rgx_native22_state_build(&out,nullptr)&&!memcmp(&out,&saved,sizeof(out)));
 assert(a04_rgx_native22_state_build((a04_rgx_native22_state_image *)data[0],&in));
 assert(a04_rgx_native22_state_build(&out,(const a04_rgx_native22_state_input *)data[0]));
 assert(!memcmp(&out,&saved,sizeof(out)));
 for(auto &d:data)for(auto byte:d)assert(byte==0xa5);
 // Generated shared word0 is a sampler, not CPU-rendered expected pixels.
 printf("PASS %u native22 shared/image/sampler/PDS-DMA configurations + failed atomic preflight\n",cases);
}
'''
class Native22State(unittest.TestCase):
    def test_actual_owned_mapping_producer_and_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.cpp').write_text(CODE)
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-Wno-parentheses','-Wno-unused-function','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),'-I',str(PDS),str(p/'case.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'arm.cpp').write_text('#include "A04BR3RogueNative22State.h"\nint build(a04_rgx_native22_state_image *o,const a04_rgx_native22_state_input *i){return a04_rgx_native22_state_build(o,i);}\n')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)
    def test_original_external_row_and_shader_metadata(self):
        directory=os.environ.get('A04_RGX_NATIVE22_EVIDENCE_DIR')
        if not directory:self.skipTest('Original REA table evidence external to source repository; set A04_RGX_NATIVE22_EVIDENCE_DIR')
        def blob(name,address):
            found=[]
            def walk(v):
                if isinstance(v,dict):
                    r=v.get('raw_result')
                    if isinstance(r,dict) and r.get('address')==address and r.get('complete'):found.append(bytes.fromhex(r['bytes_hex']))
                    for x in v.values():walk(x)
                elif isinstance(v,list):
                    for x in v:walk(x)
                elif isinstance(v,str) and v.startswith('{'):
                    try:walk(json.loads(v))
                    except ValueError:pass
            walk(json.loads((Path(directory)/name).read_text()));self.assertTrue(found);self.assertTrue(all(v==found[0] for v in found));return found[0]
        rows=blob('rea-solid-texture-tables-19a864-bytes.json','0x19a864')
        native=[rows[i:i+12] for i in range(0,len(rows),12) if int.from_bytes(rows[i:i+4],'little')==0x22]
        self.assertEqual(len(native),1);self.assertEqual(list(native[0][4:]),[1,12,8,0,1,2,3,0])
        fmt=blob('rea-solid-canonical22-197cc0-bytes.json','0x197cc0')
        self.assertEqual(int.from_bytes(fmt[24:26],'little'),0x82)
        meta=blob('rea-solid-canonical22-17eb7c-bytes.json','0x17eb7c')
        self.assertEqual((meta[1],meta[2]+1,meta[3],meta[6]&0xfe,meta[7],meta[14]),(1,9,4,0,4,8))
        self.assertEqual(int.from_bytes(meta[24:28],'little'),255)
        keys=blob('rea-solid-texture-tables-1507dc-bytes.json','0x1507dc')
        self.assertEqual(int.from_bytes(keys[:4],'little'),8)
        offsets=blob('rea-solid-canonical22-193bfc-bytes.json','0x193bfc')
        self.assertEqual((int.from_bytes(offsets[:4],'little'),int.from_bytes(offsets[4:8],'little')),(0,152))
if __name__=='__main__':unittest.main()
