# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Offline original-format22 byte-component producer, never a draw claim."""
from pathlib import Path
import subprocess
import tempfile
import unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class ClearColour(unittest.TestCase):
    def test_native_byte_components_and_fixed_task_program_resource_contract(self):
        source=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueClearColour.h"
alignas(4096) static unsigned char pc[4096],pd[3][4096],pt[3][4096],data[3][65536],bank[65536];
static a04_rgx_render_resources r;static unsigned char bankBefore[65536];
int main(){
 a04_rgx_bif heaps[3];const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL};
 for(unsigned i=0;i<3;i++)heaps[i]={{pc,0x80000000,4096},{pd[i],0x80001000+i*8192,4096},
  {pt[i],0x80002000+i*8192,4096},{data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 a04_rgx_render_buffer b={bank,0x82000000,65536};
 assert(!a04_rgx_render_build(&r,&b,&heaps[0],1));assert(!a04_rgx_heap_map_build(heaps,3));
 memcpy(bankBefore,bank,sizeof(bank));
 a04_rgx_tq_clear zero;
 assert(!a04_rgx_tq_clear_build(&zero,&r,&heaps[1],&heaps[2]));
 // Nonuniform channels exercise byte order; no modern-format-name assumption.
 for(unsigned component=0;component<4;component++)for(unsigned value=0;value<256;value++){
  unsigned c[]={0x12,0x34,0x56,0x7f};c[component]=value;
  a04_rgx_tq_clear coloured;assert(!a04_rgx_tq_clear_colour_build(&coloured,&r,&heaps[1],&heaps[2],c[0],c[1],c[2],c[3]));
  auto wire=(unsigned char *)&coloured.payload;auto base=(unsigned char *)&zero.payload;
  for(unsigned n=0;n<208;n++)assert(wire[n]==(n>=8 && n<12?c[n-8]:base[n]));
  assert(!memcmp(coloured.pixel.image,zero.pixel.image,64) && !memcmp(coloured.usc,zero.usc,12));
  // Producer never writes target, programs, CCB or queue/UFO memory.
  for(auto &h:heaps)for(unsigned n=0;n<65536;n++)assert(!data[&h-heaps][n]);
 }
 assert(!memcmp(bank,bankBefore,sizeof(bank)));
 a04_rgx_tq_clear out;memset(&out,0x6c,sizeof(out));auto unchanged=out;
 for(unsigned component=0;component<4;component++){
  unsigned c[]={0,0,0,0};c[component]=256;
  assert(a04_rgx_tq_clear_colour_build(&out,&r,&heaps[1],&heaps[2],c[0],c[1],c[2],c[3]));
  assert(!memcmp(&out,&unchanged,sizeof(out)));
 }
 assert(a04_rgx_tq_clear_colour_build(&out,&r,&heaps[1],&heaps[2],0xa5,0xa5,0xa5,0xa5));
 assert(!memcmp(&out,&unchanged,sizeof(out)));
 auto invalid=heaps[1];invalid.va=0xda00000000ULL;
 assert(a04_rgx_tq_clear_colour_build(&out,&r,&invalid,&heaps[2],1,2,3,4));assert(!memcmp(&out,&unchanged,sizeof(out)));
 assert(a04_rgx_tq_clear_colour_build((a04_rgx_tq_clear *)data[0],&r,&heaps[1],&heaps[2],1,2,3,4));
 assert(!a04_rgx_tq_clear_colour_build(&out,&r,&heaps[1],&heaps[2],0,0,0,0));
 assert(!memcmp(&out.payload,&zero.payload,208));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'arm.cpp').write_text('''#include "A04BR3RogueClearColour.h"
int encode(a04_rgx_tq_clear *o,const a04_rgx_render_resources *r,const a04_rgx_bif *p,const a04_rgx_bif *u){return a04_rgx_tq_clear_colour_build(o,r,p,u,0x12,0x34,0x56,0x7f);}
''')
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                            '-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096',
                            '-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)

if __name__=='__main__':unittest.main()
