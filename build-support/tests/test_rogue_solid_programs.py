# SPDX-License-Identifier: MIT
"""Actual CPU builder/stage dispatch, pinned source oracle and lease failures.
No shader/GPU emulator. Terminal pixels and zero-source acceptance unmeasured.
"""
from pathlib import Path
import hashlib,json,subprocess,tempfile,unittest,os
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
SHARED=HERE/'fixtures/rogue_shared'
PDS=HERE/'fixtures/rogue_pixel_event'
PCO=HERE/'fixtures/rogue_eot'
CODE=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueSolidPrograms.h"
#include "mesa_shared.h"
alignas(4096) static unsigned char pc[4096],pd[3][4096],pt[3][4096],data[3][65536],bank[65536];
static a04_rgx_render_resources r;static a04_rgx_bif heaps[3];
static unsigned holds,drops,cleans,failHold,failClean,refs;
static a04_rgx_render_buffer retained[11];
static unsigned char bankBefore[65536];
static int hold(void *,const a04_rgx_render_buffer *b){
 assert(a04_rgx_render_buffer_valid(*b));++holds;if(holds==failHold)return 1;retained[refs++]=*b;return 0;
}
static void drop(void *,const a04_rgx_render_buffer *b){assert(a04_rgx_render_buffer_valid(*b));assert(refs);assert(b->cpu==retained[refs-1].cpu&&b->pa==retained[refs-1].pa&&b->bytes==retained[refs-1].bytes);--refs;++drops;}
static int clean(void *,const a04_rgx_render_buffer *b){assert(refs==11&&holds==11&&b->cpu);++cleans;return cleans==failClean;}
static a04_rgx_solid_program_io ops={nullptr,hold,drop,clean};
static void setup(){
 const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL};
 for(unsigned i=0;i<3;i++)heaps[i]={{pc,0x80000000,4096},{pd[i],0x80001000+i*8192,4096},
  {pt[i],0x80002000+i*8192,4096},{data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 a04_rgx_render_buffer b={bank,0x82000000,65536};
 assert(!a04_rgx_render_build(&r,&b,&heaps[0],1));assert(!a04_rgx_heap_map_build(heaps,3));
 for(auto &d:data)memset(d,0x3c,sizeof(d));
 memcpy(bankBefore,bank,65536);holds=drops=cleans=refs=failHold=failClean=0;
}
static void sharedReference(){
 unsigned cases=0;uint32_t rng=0x19243;
 for(unsigned first=0;first<=252;first+=2)for(unsigned n=0;n<256;n++){
  uint32_t constants[4];for(auto &w:constants){rng=rng*1664525u+1013904223u;w=rng;}
  a04_rgx_shared_load got;uint32_t data[16]={},code[8]={};
  assert(!a04_rgx_shared_load_build(&got,sizeof(got),constants,16,first,256));
  shared_program p={constants,first,false};pvr_device_info dev={false};
  assert(mesa_shared_data(&p,data,&dev)==data+8);
  assert(mesa_shared_code(&p,code)==code+3);
  assert(!memcmp(got.image,data,32)&&!memcmp(got.image+32,code,12));
  for(unsigned i=44;i<64;i++)assert(!got.image[i]);
  assert(code[0]==0xf8040001&&code[1]==0xfc060101&&code[2]==0xd3000000);
  assert(data[4]==((first<<13)|0x10000002u)&&!data[5]);
  assert(data[6]==(((first+2)<<13)|0x90000002u)&&!data[7]);++cases;
 }
 a04_rgx_shared_load out,before;memset(&out,0xcc,sizeof(out));before=out;uint8_t c[16]={};
 for(size_t n=0;n<64;n++)assert(a04_rgx_shared_load_build(&out,n,c,16,0,4));
 for(size_t n=0;n<20;n++)if(n!=16)assert(a04_rgx_shared_load_build(&out,64,c,n,0,4));
 for(unsigned first=1;first<256;first+=2)assert(a04_rgx_shared_load_build(&out,64,c,16,first,256));
 assert(a04_rgx_shared_load_build(&out,64,c,16,254,256));
 assert(a04_rgx_shared_load_build(&out,64,c,16,0,3));
 assert(a04_rgx_shared_load_build(&out,64,c,16,0,257));
 assert(a04_rgx_shared_load_build(&out,64,c,16,0,0));
 assert(a04_rgx_shared_load_build(&out,64,NULL,16,0,4));
 assert(a04_rgx_shared_load_build(NULL,64,c,16,0,4));
 assert(a04_rgx_shared_load_build(&out,64,(void *)(UINTPTR_MAX-7),16,0,4));
 assert(a04_rgx_shared_load_build((a04_rgx_shared_load *)(UINTPTR_MAX-7),64,c,16,0,4));
 assert(a04_rgx_shared_load_build(&out,64,out.image,16,0,4));assert(!memcmp(&out,&before,64));
 printf("PASS %u native/Mesa shared4 programs and atomic failures\n",cases);
}
int main(){
 sharedReference();setup();a04_rgx_solid_program_image image;
 assert(!a04_rgx_solid_program_build(&image,sizeof(image),&r,&heaps[1],&heaps[2],0x7f563412));
 a04_rgx_tq_clear zero;assert(!a04_rgx_tq_clear_build(&zero,&r,&heaps[1],&heaps[2]));
 // Partial source-derived background fields, never a complete208 packet.
 assert(image.background.shader_branch_raw00==0x200);
 assert(image.background.raw38==(UINT64_C(160)<<32|64));
 assert(!image.background.raw40&&image.background.raw48==128);
 assert(image.background.raw50==(UINT64_C(1)<<55|UINT64_C(8)<<21|1));
 // No copied proprietary shader/table blob. Fragment is source compiled and
 // references shared0; task points to USC+64, EOT remains USC+0 unchanged.
 a04_rgx_solid_shader shader;uint8_t constants[4]={0x12,0x34,0x56,0x7f};
 assert(!a04_rgx_solid_shader_compile(&shader,sizeof(shader),constants,4,0,4,1,0));
 assert(!memcmp(image.usc,zero.usc,12)&&!memcmp(image.usc+64,shader.code,shader.code_bytes));
 assert(!memcmp(image.pds,zero.pixel.image,64));
 assert(a04_rgx_fw_u32(image.pds+64)==64&&a04_rgx_fw_u32(image.pds+96)==0xf4000002);
 assert(a04_rgx_fw_u32(image.pds+128)==0x7f563412);
 assert(!memcmp(bank,bankBefore,65536));for(auto &d:data)for(auto v:d)assert(v==0x3c);
 auto saved=image;
 for(size_t n=0;n<sizeof(image);n++)assert(a04_rgx_solid_program_build(&image,n,&r,&heaps[1],&heaps[2],1));
 assert(a04_rgx_solid_program_build(&image,sizeof(image),&r,&heaps[1],&heaps[2],0xa5a5a5a5));
 assert(a04_rgx_solid_program_build((a04_rgx_solid_program_image *)data[0],sizeof(image),&r,&heaps[1],&heaps[2],1));
 assert(!memcmp(&image,&saved,sizeof(image)));
 // Every hold failure balances earlier references, before ANY heap write.
 for(unsigned n=1;n<=11;n++){
  setup();failHold=n;A04BR3RogueSolidPrograms candidate;
  assert(candidate.stage(&r,&heaps[1],&heaps[2],1,ops)==2);
  assert(holds==n&&drops==n-1&&!refs&&!cleans&&!candidate.programsPrepared());
  assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));
  for(auto &d:data)for(auto v:d)assert(v==0x3c);
  assert(candidate.resetCold());
 }
 // Every cache failure invalidates descriptor and releases all leases. There
 // is no published CCB and no CPU-drawn substitute output, only A5 poison.
 for(unsigned n=1;n<=11;n++){
  setup();failClean=n;A04BR3RogueSolidPrograms candidate;
  assert(candidate.stage(&r,&heaps[1],&heaps[2],1,ops)==3);
  assert(holds==11&&drops==11&&!refs&&cleans==n&&!candidate.programsPrepared());
  assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));
  assert(!memcmp(bank,bankBefore,65536));for(auto v:data[0])assert(v==0xa5);
 }
 setup();{
  A04BR3RogueSolidPrograms candidate;r.control[3]->ui32WriteOffset=8;
  assert(candidate.stage(&r,&heaps[1],&heaps[2],1,ops)&&!holds);
  r.control[3]->ui32WriteOffset=0;
  assert(!candidate.stage(&r,&heaps[1],&heaps[2],0x7f563412,ops));
  assert(candidate.programsPrepared()&&holds==11&&refs==11&&cleans==11);
  assert(candidate.stage(&r,&heaps[1],&heaps[2],1,ops));
  assert(!memcmp(data[1],image.pds,192)&&!memcmp(data[2],image.usc,128));
  for(unsigned n=192;n<65536;n++)assert(data[1][n]==0x3c);
  for(unsigned n=128;n<65536;n++)assert(data[2][n]==0x3c);
  data[1][160]^=1;assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));data[1][160]^=1;
  data[2][64]^=1;assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));data[2][64]^=1;
  data[2][96]^=1;assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));data[2][96]^=1;
  data[0][65535]=0;assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2]));data[0][65535]=0xa5;
  auto shifted=heaps[1];shifted.data.pa+=4096;assert(!candidate.pinPrograms(&r,&shifted,&heaps[2]));
  assert(candidate.resetCold()&&!refs&&drops==11);
 }
 setup();{
  A04BR3RogueSolidPrograms candidate;
  assert(!candidate.stage(&r,&heaps[1],&heaps[2],0x7f563412,ops));
  const auto *programs=candidate.pinPrograms(&r,&heaps[1],&heaps[2]);assert(programs&&candidate.pinned());
  assert(programs->background.raw48==128&&programs->expected_pixel==0x7f563412);
  assert(!memcmp(bank,bankBefore,65536)); // no TASK/UPDATE/CCB publication
  assert(!candidate.pinPrograms(&r,&heaps[1],&heaps[2])&&!candidate.resetCold());
  assert(refs==11&&!drops); // immutable programs/pages/shared allocation alive
 }
 assert(refs==11&&!drops); // destructor also quarantines firmware-referenced pages
 puts("PASS source builder -> owned leases/cache/programs -> bounded partial old208 fields; no task or GPU simulation");
}
'''
class SolidPrograms(unittest.TestCase):
    def test_coherent_owned_builder_oracle_and_lifetimes(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.cpp').write_text(CODE)
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),'-I',str(SHARED),'-I',str(PDS),str(p/'case.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'arm.cpp').write_text('''#include "A04BR3RogueSolidPrograms.h"
int stage(A04BR3RogueSolidPrograms *o,const a04_rgx_render_resources *r,const a04_rgx_bif *p,const a04_rgx_bif *u,const a04_rgx_solid_program_io *io){return o->stage(r,p,u,0x7f563412,*io);}
const a04_rgx_solid_program_image *pin(A04BR3RogueSolidPrograms *o,const a04_rgx_render_resources *r,const a04_rgx_bif *p,const a04_rgx_bif *u){return o->pinPrograms(r,p,u);}
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)
            (p/'shared.c').write_text('#include "A04BR3RogueSharedLoad.h"\nint load(struct a04_rgx_shared_load *o,const void *c){return a04_rgx_shared_load_build(o,sizeof(*o),c,16,0,4); }\n')
            subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-I',str(GPU),'-c',str(p/'shared.c'),'-o',str(p/'shared.o')],check=True)
    def test_primary_nop_forward_source_and_optional_native_evidence(self):
        source=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pco_isa.h"
#include "A04BR3RoguePrimaryNop.h"
int main(int argc,char **argv){
 uint8_t ref[8]={0},out[10];memset(out,0xa5,10);
 unsigned n=pco_igrp_hdr_control_encode(ref,.da=0,.length=4,.olchk=false,.w1p=false,.w0p=false,.cc=PCO_CC_E1_ZX,.miscctl=1,.ctrlop=PCO_CTRLOP_NOP);
 n+=pco_ctrl_nop_encode(ref+n);assert(n==4);
 ref[n++]=0xf0|2;while(n<8)ref[n++]=0xff;
 assert(!a04_rgx_primary_nop_generate(out+1,8)&&!memcmp(out+1,ref,8));
 assert(out[0]==0xa5&&out[9]==0xa5);
 memset(out,0xa5,10);
 for(size_t bytes=0;bytes<8;bytes++)assert(a04_rgx_primary_nop_generate(out+1,bytes));
 for(unsigned i=0;i<10;i++)assert(out[i]==0xa5);
 assert(a04_rgx_primary_nop_generate(NULL,8));
 assert(a04_rgx_primary_nop_generate((void *)(UINTPTR_MAX-3),8));
 if(argc==2){FILE *f=fopen(argv[1],"rb");uint8_t native[8];assert(f);
  assert(fread(native,1,8,f)==8&&fgetc(f)==EOF);fclose(f);assert(!memcmp(native,ref,8));
  puts("PASS source-built primary8 exactly equals external native REA evidence");}
 puts("PASS named PCO NOP.END forward assembly + bounds/guards");return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'nop.c').write_text(source)
            subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-override-init','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),'-I',str(PCO),str(p/'nop.c'),'-o',str(p/'test')],check=True)
            args=[str(p/'test')]
            evidence=os.environ.get('A04_RGX_PRIMARY_NOP_EVIDENCE')
            if evidence:
                values=[]
                def walk(v):
                    if isinstance(v,dict):
                        r=v.get('raw_result')
                        if isinstance(r,dict) and r.get('address')=='0x150604' and r.get('complete') and r.get('returned_bytes')==8:
                            values.append(bytes.fromhex(r['bytes_hex']))
                        for item in v.values():walk(item)
                    elif isinstance(v,list):
                        for item in v:walk(item)
                    elif isinstance(v,str) and v.startswith('{'):
                        try:walk(json.loads(v))
                        except ValueError:pass
                walk(json.loads(Path(evidence).read_text()))
                self.assertTrue(values);self.assertTrue(all(x==values[0] and len(x)==8 for x in values))
                (p/'external.bin').write_bytes(values[0]);args.append(str(p/'external.bin'))
            subprocess.run(args,check=True)
    def test_fixture_provenance(self):
        m=json.loads((SHARED/'provenance.json').read_text())
        self.assertEqual(m['revision'],'e1ef8f046c6e37ec1e2a13229560fbb824e5118f')
        self.assertEqual(hashlib.sha256((SHARED/'mesa_shared.h').read_bytes()).hexdigest(),m['fixture_sha256'])
        m=json.loads((PCO/'provenance.json').read_text())
        for path,digest in m['fixture_sha256'].items():self.assertEqual(hashlib.sha256((PCO/path).read_bytes()).hexdigest(),digest)
        m=json.loads((PDS/'provenance.json').read_text())
        self.assertEqual(hashlib.sha256((PDS/'mesa_pixel_event.h').read_bytes()).hexdigest(),m['fixture_sha256'])
if __name__=='__main__':unittest.main()
