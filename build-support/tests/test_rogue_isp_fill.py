# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual CPU producer/mapping checks. No GPU/firmware execution is simulated."""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU = Path(__file__).resolve().parents[2] / 'iokit/Kernel/arm/rogue'
CASE = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueISPFill.h"
alignas(4096) static uint8_t pc[4096],pd[4][4096],pt[4][4096],data[4][65536];
static uint8_t savedPC[4096],savedPD[sizeof(pd)],savedPT[sizeof(pt)];
static a04_rgx_isp_fill_input in;
static uint64_t u64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static uint32_t u32(const uint8_t *p){return (uint32_t)u64(p);}
static void setup(uint64_t ispVA=0){
 const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL,ispVA};
 in={};
 for(unsigned i=0;i<4;i++)in.heaps[i]={{pc,0x80000000,4096},
  {pd[i],0x80010000+i*8192,4096},{pt[i],0x80011000+i*8192,4096},
  {data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 assert(!a04_rgx_heap_map_build(in.heaps,4));
 memset(data,0xa5,sizeof(data));
 in.target={va[0],4096};in.fragment_pds={va[1]+64,36};in.fragment_usc={va[2]+64,8};
 in.control={ispVA,128};in.quad={ispVA+128,88};
 in.native_format=0x22;in.width=in.height=in.pitch_pixels=32;
 in.x1=in.y1=32;in.pds_temps=1;
 memcpy(savedPC,pc,sizeof(pc));memcpy(savedPD,pd,sizeof(pd));memcpy(savedPT,pt,sizeof(pt));
}
static void unchanged(){
 assert(!memcmp(savedPC,pc,sizeof(pc)) && !memcmp(savedPD,pd,sizeof(pd)) && !memcmp(savedPT,pt,sizeof(pt)));
 for(unsigned i=0;i<sizeof(data);i++)assert(((uint8_t *)data)[i]==0xa5);
}
static void golden(){
 setup();a04_rgx_isp_fill_image out;memset(&out,0x6c,sizeof(out));
 assert(!a04_rgx_isp_fill_build(&out,&in));
 // Fixed native FUN12dc50 one-quad scalar oracle. No production helpers.
 const uint64_t words[]={0x60,0x2e0000,0x14c718a404430820ULL,
  0x100008000ULL,0x100008200ULL,0x104008000ULL,0x104008200ULL,
  64,1,0,0x4000};
 for(unsigned i=0;i<11;i++)assert(u64(out.quad+i*8)==words[i]);
 assert(u64(out.control)==0x210000032aULL);
 for(unsigned i=8;i<128;i++)assert(!out.control[i]);
 assert(out.raw60==2 && out.raw68==0 && out.raw70==0 && out.raw78==0);
 assert(out.control_destination.va==0 && out.control_destination.bytes==128);
 assert(out.quad_destination.va==128 && out.quad_destination.bytes==88);unchanged();
 assert(out.target_reference.va==in.target.va && out.target_reference.bytes==4096);
 assert(out.fragment_pds_reference.va==in.fragment_pds.va && out.fragment_pds_reference.bytes==36);
 assert(out.fragment_usc_reference.va==in.fragment_usc.va && out.fragment_usc_reference.bytes==8);
}
static void valid(){
 unsigned cases=0;
 for(unsigned high=0;high<2;high++){
  setup(high?0x3fff0000:0);
  for(unsigned temp=0;temp<=60;temp++)for(unsigned c=0;c<3;c++)for(unsigned q=0;q<3;q++){
   const unsigned controls[]={0,8,0xff80},quads[]={0x80,0x188,0xffa8};
   const unsigned pds[]={0,16,0xffd0},usc[]={0,4,0xfff8};
   in.control.va=in.heaps[3].va+controls[c];in.quad.va=in.heaps[3].va+quads[q];
   if(a04_rgx_fw_overlap(in.control.va,128,in.quad.va,88))continue;
   in.fragment_pds.va=in.heaps[1].va+pds[q];in.fragment_usc.va=in.heaps[2].va+usc[q];
   in.target.va=in.heaps[0].va+(q==2?61440:128*q);in.pds_temps=temp;
   a04_rgx_isp_fill_image out;assert(!a04_rgx_isp_fill_build(&out,&in));++cases;
   assert(u64(out.control)==(((in.quad.va>>3)<<33)|0x10000032aULL));
   // Decode the native branch/control reference, not a manufactured completion.
   assert((u64(out.control)>>33)*8==in.quad.va);
   assert(u32(out.quad+56)==pds[q] && u32(out.quad+64)==(temp+3)/4);
   assert(u32(out.quad+60)==0 && u32(out.quad+68)==0 && u64(out.quad+72)==0);
   assert(u64(out.quad+80)==0x4000);
   assert(out.raw68==in.control.va && out.raw70==0);unchanged();
  }
 }
 assert(cases==854);printf("validated layouts=%u\n",cases);
}
static void invalid(){
 setup();a04_rgx_isp_fill_image out;memset(&out,0x6c,sizeof(out));const auto before=out;
 for(unsigned kind=0;kind<40;kind++){
  auto bad=in;
  if(kind==0)bad.native_format=0x42475241;
  if(kind==1)bad.width=31;
  if(kind==2)bad.height=33;
  if(kind==3)bad.pitch_pixels=64;
  if(kind==4)bad.source_count=1;
  if(kind==5)bad.x0=-2049;
  if(kind==6)bad.y0=6144;
  if(kind==7)bad.x1=0;
  if(kind==8)bad.y1=INT32_MIN;
  if(kind==9)bad.pds_temps=61;
  if(kind==10)bad.pds_temps=UINT32_MAX;
  if(kind==11)bad.heaps[3].va=0xe400000000ULL;
  if(kind==12)bad.heaps[3].va=0x40000000;
  if(kind==13)bad.heaps[3].va=0x3ffff000;
  if(kind==14)bad.heaps[0].read_only=true;
  if(kind==15)bad.heaps[1].read_only=true;
  if(kind==16)bad.heaps[2].read_only=false;
  if(kind==17)bad.heaps[3].read_only=true;
  if(kind==18)bad.heaps[1].pc.pa+=4096;
  if(kind==19)bad.heaps[3].data=bad.heaps[0].data;
  if(kind==20)bad.target.bytes=4095;
  if(kind==21)bad.target.va++;
  if(kind==22)bad.target.va+=65536;
  if(kind==23)bad.fragment_pds.bytes=32;
  if(kind==24)bad.fragment_pds.va++;
  if(kind==25)bad.fragment_pds.va=0xa80000fff0ULL;
  if(kind==26)bad.fragment_usc.bytes=4;
  if(kind==27)bad.fragment_usc.va++;
  if(kind==28)bad.fragment_usc.va=UINT64_MAX-3;
  if(kind==29)bad.fragment_usc.bytes=UINT64_MAX;
  if(kind==30)bad.control.bytes=127;
  if(kind==31)bad.control.va=4;
  if(kind==32)bad.quad.bytes=89;
  if(kind==33)bad.quad.va=129;
  if(kind==34)bad.quad.va=120;
  if(kind==35)bad.quad.va=65536-80;
  if(kind==36)bad.control.va=65536-120;
  if(kind==37)bad.heaps[0].va+=0x40000000;
  if(kind==38)bad.heaps[1].va=0xda00000000ULL;
  if(kind==39)bad.heaps[2].va=0xbc00000000ULL;
  const auto saved=bad;assert(a04_rgx_isp_fill_build(&out,&bad));
  assert(!memcmp(&bad,&saved,sizeof(bad)) && !memcmp(&out,&before,sizeof(out)));unchanged();
 }
 // Corrupt mapped PTE, not just metadata; no emission on a bad mapping.
 auto table=(uint64_t *)pt[3];table[0]^=1;
 assert(a04_rgx_isp_fill_build(&out,&in));assert(!memcmp(&out,&before,sizeof(out)));table[0]^=1;
 // Output, descriptor and GPU backing memory must not alias.
 assert(a04_rgx_isp_fill_build((a04_rgx_isp_fill_image *)data[0],&in));unchanged();
 memcpy(data[0],&in,sizeof(in));
 assert(a04_rgx_isp_fill_build(&out,(a04_rgx_isp_fill_input *)data[0]));
 assert(!memcmp(data[0],&in,sizeof(in)));memset(data[0],0xa5,sizeof(in));unchanged();
 union alignas(8) Alias{a04_rgx_isp_fill_image out;a04_rgx_isp_fill_input input;} alias;
 alias.input=in;uint8_t bytes[sizeof(alias)];memcpy(bytes,&alias,sizeof(alias));
 assert(a04_rgx_isp_fill_build(&alias.out,&alias.input));assert(!memcmp(bytes,&alias,sizeof(alias)));
 assert(a04_rgx_isp_fill_build(nullptr,&in) && a04_rgx_isp_fill_build(&out,nullptr));
}
int main(){golden();valid();invalid();}
'''


class RogueISPFill(unittest.TestCase):
    def test_compiled_matching_mt8167_heap_profile(self):
        ddk = Path(__file__).resolve().parents[4] / (
            'gsi/kernel-4.14/drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693')
        if not ddk.exists():
            self.skipTest('Matching pinned DDK source not present; CPU producer tests remain independent')
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'profile.cpp').write_text(r'''
#include <stdint.h>
#define IMG_UINT64_C(x) UINT64_C(x)
#include "config_kernel_user_mt8167.h"
#include "rgxheapconfig.h"
#include "A04BR3RogueISPFill.h"
#ifndef FIX_HW_BRN_65273
#error MT8167 compiled core did not select BRN65273
#endif
static_assert(RGX_BVNC_KM_B==22 && RGX_BVNC_KM_V==40 && RGX_BVNC_KM_N==54 && RGX_BVNC_KM_C==30,"board core");
static_assert(A04_RGX_ISP_HEAP_BASE==RGX_TQ3DPARAMETERS_BRN_65273_HEAP_BASE,"ISP heap base");
static_assert(A04_RGX_ISP_HEAP_BYTES==RGX_TQ3DPARAMETERS_BRN_65273_HEAP_SIZE,"ISP heap size");
''')
            subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ddk / 'include'), '-I', str(ddk / 'hwdefs/km'),
                            '-I', str(ddk / 'services/server/devices/rgx'), '-I', str(GPU),
                            '-c', str(p / 'profile.cpp'), '-o', str(p / 'profile.o')], check=True)

    def test_native_control_quad_resources_and_atomic_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'case.cpp').write_text(CASE)
            subprocess.run(['c++', '-std=c++14', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-I', str(GPU), str(p / 'case.cpp'), '-o', str(p / 'case')], check=True)
            subprocess.run([str(p / 'case')], check=True)

    def test_freestanding_arm64_caller(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'arm.cpp').write_text('#include "A04BR3RogueISPFill.h"\n'
                                      'int encode(a04_rgx_isp_fill_image *o,const a04_rgx_isp_fill_input *i)'
                                      '{return a04_rgx_isp_fill_build(o,i);}\n')
            subprocess.run(['clang++', '--target=arm64-apple-darwin', '-std=c++14',
                            '-ffreestanding', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wframe-larger-than=4096', '-I', str(GPU), '-c', str(p / 'arm.cpp'),
                            '-o', str(p / 'arm.o')], check=True)


if __name__ == '__main__':
    unittest.main()
