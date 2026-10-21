# SPDX-License-Identifier: MIT
"""Forward compiler verification, not native shader disassembly/emulation.
Compare real pinned PCO encoders + exact original final-alignment function.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
EOT=HERE/'fixtures/rogue_eot'
SOLID=HERE/'fixtures/rogue_solid'
CODE=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pco_isa.h"
#include "A04BR3RogueSolidShader.h"
#define ROGUE_ICACHE_ALIGN 8U
// Only fields used by actual source calc_align_padding. Program byte encoding
// is provided separately by the ORIGINAL generated PCO encoder functions.
typedef struct pco_igrp {
 struct {struct {unsigned total,align_padding;} len;unsigned offset;} enc;
 struct {unsigned length;} hdr;
 struct pco_igrp *previous;
} pco_igrp;
static pco_igrp *pco_prev_igrp(pco_igrp *g){return g->previous;}
#include "pco_align.h"
static unsigned reference(uint8_t *out,unsigned first,unsigned count,unsigned pixel){
 pco_igrp groups[4]={0};unsigned bytes=count*10;
 for(unsigned i=0;i<count;i++){groups[i].enc.len.total=10;groups[i].enc.offset=i*10;groups[i].hdr.length=5;groups[i].previous=i?&groups[i-1]:NULL;}
 calc_align_padding(&groups[count-1],&bytes);
 unsigned at=0;
 for(unsigned i=0;i<count;i++){
  unsigned n=0;uint8_t *p=out+at;
  n+=pco_igrp_hdr_main_encode(p+n,.da=3,.length=groups[i].hdr.length,.oporg=PCO_OPORG_P0,
      .olchk=true,.w0p=true,.w1p=false,.cc=PCO_CC_E1_ZX,.end=i==count-1,.atom=false,.rpt=1);
  n+=pco_main_sngl_encode(p+n,.sngl_op=PCO_SNGL_OP_BYP);
  n+=pco_src_1lo_3b11i_2m_encode(p+n,.sb0=PCO_REGBANK_SHARED,.s0=first+i,.is0=PCO_IS0_SEL_S0);
  n+=pco_src_1up_1b6i_encode(p+n,.sb3=PCO_REGBANK_SPECIAL,.s3=0);
  n+=pco_iss_iss_encode(p+n,.is1=0,.is2=0,.is3=0,.is4=PCO_IS4_SEL_FT0,.is5=0);
  n+=pco_dst_1_1b6i_encode(p+n,.dbN=PCO_REGBANK_SPECIAL,.dN=PCO_SR_PIXOUT0+pixel+i);
  assert(n==10);
  unsigned padding=groups[i].enc.len.align_padding;
  if(padding){p[n++]=(uint8_t)(0xf0|padding/2);for(unsigned j=1;j<padding;j++)p[n++]=0xff;}
  assert(n==groups[i].enc.len.total);at+=n;
 }
 assert(at==bytes&&!(bytes%8));assert(!(groups[count-1].enc.offset%8));return bytes;
}
int main(void){
 unsigned cases=0;uint8_t constants[17];for(unsigned i=0;i<sizeof(constants);i++)constants[i]=(uint8_t)(i*71+23);
 struct a04_rgx_solid_shader got,old;uint8_t expected[48];
 for(unsigned capacity=1;capacity<=256;capacity++)for(unsigned first=0;first<capacity;first++)
  for(unsigned count=1;count<=4;count++)for(unsigned pixel=0;pixel<4;pixel++){
   memset(&got,0xa5,sizeof(got));old=got;memset(expected,0,sizeof(expected));
   int rc=a04_rgx_solid_shader_compile(&got,sizeof(got),constants+1,count*4,first,capacity,count,pixel);
   if(first+count>capacity || pixel+count>4){assert(rc&&!memcmp(&got,&old,sizeof(got)));continue;}
   assert(!rc);unsigned n=reference(expected,first,count,pixel);
   assert(got.code_bytes==n&&!memcmp(got.code,expected,48));
   assert(got.shared_first==first&&got.shared_words==count&&got.pixel_first==pixel&&got.pixel_words==count&&!got.temps);
   assert(!memcmp(got.shared_data,constants+1,count*4));for(unsigned i=count*4;i<16;i++)assert(!got.shared_data[i]);++cases;
  }
 // All external buffers are bounded BEFORE any read/write, including tiny
 // output and unaligned/overflow/alias attempts; rejections are atomic.
 memset(&got,0xa5,sizeof(got));old=got;
 for(size_t n=0;n<sizeof(got);n++){assert(a04_rgx_solid_shader_compile(&got,n,constants,4,0,1,1,0));assert(!memcmp(&got,&old,sizeof(got)));}
 for(size_t n=0;n<=20;n++)if(n!=4){assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,n,0,1,1,0));assert(!memcmp(&got,&old,sizeof(got)));}
 assert(a04_rgx_solid_shader_compile(NULL,sizeof(got),constants,4,0,1,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),NULL,4,0,1,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,4,0,0,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,4,0,257,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,4,UINT32_MAX,256,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,4,0,256,0,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,20,0,256,5,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),constants,4,0,256,1,UINT32_MAX));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),(void *)(UINTPTR_MAX-1),4,0,1,1,0));
 assert(a04_rgx_solid_shader_compile((struct a04_rgx_solid_shader *)(UINTPTR_MAX-3),sizeof(got),constants,4,0,1,1,0));
 assert(a04_rgx_solid_shader_compile((struct a04_rgx_solid_shader *)((uint8_t*)&got+1),sizeof(got),constants,4,0,1,1,0));
 assert(a04_rgx_solid_shader_compile(&got,sizeof(got),got.code+10,4,0,1,1,0));assert(!memcmp(&got,&old,sizeof(got)));
 for(unsigned n=0;n<16;n++)if(n!=10&&n!=12&&n!=14){uint8_t buf[16];memset(buf,0xa5,16);assert(a04_rgx_solid_emit(buf,sizeof(buf),0,0,0,n));for(unsigned j=0;j<16;j++)assert(buf[j]==0xa5);}
 // Standalone bounded group emitter also rejects invalid capacities/fields.
 uint8_t single[18],singleBefore[18];memset(single,0xa5,sizeof(single));memcpy(singleBefore,single,sizeof(single));
 for(unsigned n=0;n<16;n++){assert(a04_rgx_solid_emit(single+1,n,0,0,1,16));assert(!memcmp(single,singleBefore,sizeof(single)));}
 assert(a04_rgx_solid_emit(single+1,16,256,0,1,16));assert(a04_rgx_solid_emit(single+1,16,0,4,1,16));assert(a04_rgx_solid_emit(single+1,16,0,0,2,16));assert(a04_rgx_solid_emit(NULL,16,0,0,1,16));assert(!memcmp(single,singleBefore,sizeof(single)));
 assert(!a04_rgx_solid_emit(single+1,16,255,3,1,16));assert(single[0]==0xa5&&single[17]==0xa5);
 // The public compile writes its exact struct even if a larger capacity is
 // advertised; every byte outside it is a preserved guard.
 struct {uint32_t before;struct a04_rgx_solid_shader shader;uint32_t after;} guard;
 memset(&guard,0xa5,sizeof(guard));assert(!a04_rgx_solid_shader_compile(&guard.shader,sizeof(guard),constants,4,0,1,1,0));assert(guard.before==0xa5a5a5a5&&guard.after==0xa5a5a5a5);
 // Byte-exact raw32 constants have no CPU-side PBE/target write side effect.
 for(unsigned i=0;i<sizeof(constants);i++)assert(constants[i]==(uint8_t)(i*71+23));
 printf("PASS %u pinned-PCO fragment programs plus bounds/atomic failures\n",cases);return 0;
}
'''
class SolidShader(unittest.TestCase):
    def test_source_encoder_equivalence_and_atomic_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.c').write_text(CODE)
            subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-override-init','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),'-I',str(EOT),'-I',str(SOLID),str(p/'case.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            # The compile API is C11/C++14 and independent of reference runtime.
            caller='#include "A04BR3RogueSolidShader.h"\nint run(struct a04_rgx_solid_shader *o,const void *c){return a04_rgx_solid_shader_compile(o,sizeof(*o),c,4,0,1,1,0);}\n'
            (p/'caller.c').write_text(caller)
            subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-I',str(GPU),'-c',str(p/'caller.c'),'-o',str(p/'arm.o')],check=True)
            subprocess.run(['c++','-std=c++14','-ffreestanding','-Wall','-Wextra','-Werror','-I',str(GPU),'-x','c++','-fsyntax-only',str(p/'caller.c')],check=True)
    def test_pinned_reference_provenance(self):
        m=json.loads((EOT/'provenance.json').read_text())
        self.assertEqual(m['revision'],'e1ef8f046c6e37ec1e2a13229560fbb824e5118f')
        for path,digest in m['fixture_sha256'].items():self.assertEqual(hashlib.sha256((EOT/path).read_bytes()).hexdigest(),digest)
        m=json.loads((SOLID/'provenance.json').read_text())
        self.assertEqual(hashlib.sha256((SOLID/'pco_align.h').read_bytes()).hexdigest(),m['fixture_sha256']['pco_align.h'])
if __name__=='__main__':unittest.main()
