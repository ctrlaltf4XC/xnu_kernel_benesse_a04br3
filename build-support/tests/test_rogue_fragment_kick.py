# SPDX-License-Identifier: MIT
"""Forward-producer comparison only: no GPU emulation or submission."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
FIXTURE=HERE/'fixtures/rogue_fragment'
DEPENDENCY=HERE/'fixtures/rogue_pixel_event'
CODE=r'''
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueFragmentKick.h"
#include "mesa_fragment_kick.h"
static void compare(uint32_t address,unsigned temps,unsigned rate){
 struct a04_rgx_fragment_kick got;
 uint32_t ref[16]={0};struct pvr_pds_kickusc_program p={};
 memset(&got,0xa5,sizeof(got));
 assert(!a04_rgx_fragment_kick_build(&got,sizeof(got),address,4,temps,rate));
 pvr_pds_setup_doutu(&p.usc_task_control,address,temps,rate,false);
 assert(!pvr_pds_kick_usc(&p,NULL,0,false,PDS_GENERATE_SIZES));
 assert(p.data_size==8&&p.code_size==1);
 assert(pvr_pds_kick_usc(&p,ref,0,false,PDS_GENERATE_CODEDATA_SEGMENTS)==ref+9);
 assert(!memcmp(&got,ref,64));
 assert(ref[8]==0xf4000002);
 for(unsigned i=2;i<8;i++)assert(!ref[i]);
 for(unsigned i=9;i<16;i++)assert(!ref[i]);
 // Independent native task formula from REA FUN_0013c5f8, restricted to
 // accepted aligned/nontruncating inputs. This verifies OLD target too.
 uint64_t native=address;
 if(temps)native|=(((uint64_t)temps<<33)+UINT64_C(0x600000000))&UINT64_C(0x1f800000000);
 native|=(uint64_t)rate<<33;
 assert(ref[0]==(uint32_t)native&&ref[1]==(uint32_t)(native>>32));
 // Native FUN_0013cd1c END and constant index0/cc0; no inferred USC words.
 assert(ref[8]==((0u&0xfeu)<<7 | 0u<<27 | 0xf4000002u));
}
int main(void){
 unsigned cases=0;uint32_t random=0x927345ab;
 for(unsigned temps=0;temps<=252;temps++)for(unsigned rate=0;rate<=2;rate+=2){
  compare(0,temps,rate);compare(0xfffffffc,temps,rate);cases+=2;
  for(unsigned n=0;n<256;n++){
   random=random*1664525u+1013904223u;compare(random&~3u,temps,rate);++cases;
  }
 }
 struct a04_rgx_fragment_kick got,saved;
 memset(&got,0xa5,sizeof(got));saved=got;
 for(size_t n=0;n<64;n++)assert(a04_rgx_fragment_kick_build(&got,n,0,4,0,0));
 assert(!memcmp(&got,&saved,sizeof(got)));
 for(unsigned n=1;n<4;n++)assert(a04_rgx_fragment_kick_build(&got,64,n,4,0,0));
 for(unsigned n=0;n<8;n++)if(!n||(n&3))assert(a04_rgx_fragment_kick_build(&got,64,0,n,0,0));
 assert(a04_rgx_fragment_kick_build(NULL,64,0,4,0,0));
 assert(a04_rgx_fragment_kick_build((struct a04_rgx_fragment_kick *)(UINTPTR_MAX-63),64,0,4,0,0));
 assert(a04_rgx_fragment_kick_build(&got,64,UINT64_MAX,4,0,0));
 assert(a04_rgx_fragment_kick_build(&got,64,UINT64_C(0x100000000),4,0,0));
 assert(a04_rgx_fragment_kick_build(&got,64,0xfffffffc,8,0,0));
 assert(a04_rgx_fragment_kick_build(&got,64,0,SIZE_MAX,0,0));
 for(unsigned n=253;n<=256;n++)assert(a04_rgx_fragment_kick_build(&got,64,0,4,n,0));
 assert(a04_rgx_fragment_kick_build(&got,64,0,4,UINT32_MAX,0));
 for(unsigned n=1;n<10;n++)if(n!=2)assert(a04_rgx_fragment_kick_build(&got,64,0,4,0,n));
 assert(a04_rgx_fragment_kick_build(&got,64,0,4,0,UINT32_MAX));
 assert(!memcmp(&got,&saved,sizeof(got)));
 struct {uint8_t before;struct a04_rgx_fragment_kick out;uint8_t after;} guards;
 memset(&guards,0xa5,sizeof(guards));
 assert(!a04_rgx_fragment_kick_build(&guards.out,sizeof(guards),0,16,0,0));
 assert(guards.before==0xa5&&guards.after==0xa5);
 printf("PASS %u pinned-Mesa/native fragment launches + atomic bounds\n",cases);
 return 0;
}
'''
class FragmentKick(unittest.TestCase):
    def test_source_encoder_and_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.c').write_text(CODE)
            subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),'-I',str(FIXTURE),'-I',str(DEPENDENCY),str(p/'case.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            caller='#include "A04BR3RogueFragmentKick.h"\nint run(struct a04_rgx_fragment_kick *o){return a04_rgx_fragment_kick_build(o,sizeof(*o),16,16,0,0);}\n'
            (p/'caller.c').write_text(caller)
            subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-I',str(GPU),'-c',str(p/'caller.c'),'-o',str(p/'arm.o')],check=True)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-ffreestanding','-I',str(GPU),'-x','c++','-fsyntax-only',str(p/'caller.c')],check=True)
    def test_fixture_provenance(self):
        m=json.loads((FIXTURE/'provenance.json').read_text())
        self.assertEqual(m['revision'],'e1ef8f046c6e37ec1e2a13229560fbb824e5118f')
        self.assertEqual(hashlib.sha256((FIXTURE/'mesa_fragment_kick.h').read_bytes()).hexdigest(),m['fixture_sha256'])
        m=json.loads((DEPENDENCY/'provenance.json').read_text())
        self.assertEqual(hashlib.sha256((DEPENDENCY/'mesa_pixel_event.h').read_bytes()).hexdigest(),m['fixture_sha256'])
if __name__=='__main__':unittest.main()
