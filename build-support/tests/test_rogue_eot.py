# SPDX-License-Identifier: MIT
"""Forward USC assembly against unmodified pinned PCO-generated encoders.
No native byte decoding, opaque shader fixture, GPU emulation or physical test.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
FIXTURE=HERE/'fixtures/rogue_eot'
class RogueEOT(unittest.TestCase):
    def test_generated_fixture_integrity(self):
        m=json.loads((FIXTURE/'provenance.json').read_text())
        self.assertEqual(m['revision'],'e1ef8f046c6e37ec1e2a13229560fbb824e5118f')
        for name,digest in m['fixture_sha256'].items():
            self.assertEqual(hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest(),digest)
    def test_source_assembly_and_bounded_output(self):
        src=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "pco_isa.h"
#include "A04BR3RogueEOT.h"
int main(void){
 uint8_t expected[12]={0};unsigned n=0;
 /* O_EMITPIX group map from pinned pco_map.py; EOT olchk_skip=true,
  * pco_end=true. pco_group_instrs calc_lengths=11+odd-byte pad, DA=4. */
 n+=pco_igrp_hdr_main_encode(expected+n,.da=4,.length=6,.oporg=PCO_OPORG_BE,
     .olchk=false,.w0p=false,.w1p=false,.cc=PCO_CC_E1_ZX,.end=true,.atom=false,.rpt=1);
 n+=pco_backend_emitpix_encode(expected+n,.freep=true);
 n+=pco_src_3lo_3b8i_2b8i_3b8i_3m_encode(expected+n,.sb0=PCO_REGBANK_SHARED,.s0=0,
     .sb1=PCO_REGBANK_SPECIAL,.s1=0,.sb2=PCO_REGBANK_SHARED,.s2=1,.is0=PCO_IS0_SEL_S0);
 n+=pco_src_1up_1b6i_encode(expected+n,.sb3=PCO_REGBANK_SPECIAL,.s3=0);
 n+=pco_iss_iss_encode(expected+n,.is1=0,.is2=0,.is3=0,.is4=0,.is5=0);
 assert(n==11);expected[n++]=0xff;assert(n==12);
 assert(A04_RGX_EOT_SINGLE_RT_BYTES==12 && A04_RGX_EOT_SINGLE_RT_TEMPS==0);
 uint8_t buf[32],before[32];memset(buf,0xa5,sizeof(buf));memcpy(before,buf,sizeof(buf));
 for(unsigned capacity=0;capacity<12;capacity++){
  assert(a04_rgx_eot_single_rt_generate(buf+1,capacity));assert(!memcmp(buf,before,sizeof(buf)));
 }
 assert(a04_rgx_eot_single_rt_generate(0,12));
 assert(!a04_rgx_eot_single_rt_generate(buf+1,12));assert(!memcmp(buf+1,expected,12));
 assert(buf[0]==0xa5);for(unsigned i=13;i<32;i++)assert(buf[i]==0xa5);
 memset(buf,0,sizeof(buf));assert(!a04_rgx_eot_single_rt_generate(buf,32));
 assert(!memcmp(buf,expected,12));for(unsigned i=12;i<32;i++)assert(!buf[i]);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'test.c').write_text(src)
            subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-I',str(GPU),'-I',str(FIXTURE),str(p/'test.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
if __name__=='__main__':unittest.main()
