# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Cross-unit source contract only; no accepted208, heap staging or GPU model.
Keep primary NOP dispatch distinct from the shared-solid fragment dispatch.
"""
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
CODE=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueSolidPrograms.h"
#include "A04BR3RogueISPFill.h"
#include "A04BR3RogueNative22State.h"
alignas(4096) static uint8_t pc[4096],pd[4][4096],pt[4][4096],data[4][65536],bank[65536];
static a04_rgx_render_resources r;
static a04_rgx_isp_fill_input in;
struct combined {a04_rgx_solid_program_image programs;a04_rgx_fragment_kick primary;a04_rgx_isp_fill_image isp;};
/* Test-only consumer glue: output commits only after all real source builders
 * pass. No fake texture/background resources and no attempted full208. */
static int compile(combined *out,const a04_rgx_isp_fill_input &input){
 combined made={};
 if(a04_rgx_solid_program_build(&made.programs,sizeof(made.programs),&r,&input.heaps[1],&input.heaps[2],0x7f563412))return 1;
 if(a04_rgx_fragment_kick_build(&made.primary,sizeof(made.primary),A04_RGX_SOLID_PRIMARY_USC,A04_RGX_PRIMARY_NOP_BYTES,0,0))return 1;
 if(a04_rgx_isp_fill_build(&made.isp,&input))return 1;
 *out=made;return 0;
}
static void setup(){
 in={};const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL,0x10000};
 for(unsigned i=0;i<4;i++)in.heaps[i]={{pc,0x80000000,4096},
  {pd[i],0x80010000+i*8192,4096},{pt[i],0x80011000+i*8192,4096},
  {data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 a04_rgx_render_buffer b={bank,0x82000000,65536};
 assert(!a04_rgx_render_build(&r,&b,&in.heaps[0],1));assert(!a04_rgx_heap_map_build(in.heaps,4));
 memset(data,0xa5,sizeof(data));
 in.target={va[0],4096};in.fragment_pds={va[1]+192,64};in.fragment_usc={va[2]+96,8};
 in.control={va[3],128};in.quad={va[3]+128,88};
 in.native_format=0x22;in.width=in.height=in.pitch_pixels=32;in.x1=in.y1=32;
}
int main(){
 setup();unsigned cases=0;combined out;uint8_t sourceNop[8];
 assert(!a04_rgx_primary_nop_generate(sourceNop,sizeof(sourceNop)));
 for(unsigned temp=0;temp<=60;temp++){
  in.pds_temps=temp;assert(!compile(&out,in));++cases;
  // Separate primary at PDS+192/USC+96; shared-solid remains PDS+64/USC+64.
  assert(a04_rgx_fw_u32(out.primary.image)==96);
  assert(!a04_rgx_fw_u32(out.primary.image+4));
  assert(a04_rgx_fw_u32(out.primary.image+32)==0xf4000002);
  assert(!memcmp(out.programs.usc+96,sourceNop,8));
  assert(a04_rgx_fw_u32(out.programs.pds+64)==64);
  assert(out.isp.fragment_pds_reference.va==in.heaps[1].va+192);
  assert(out.isp.fragment_usc_reference.va==in.heaps[2].va+96);
  assert(a04_rgx_fw_u32(out.isp.quad+56)==192);
  assert(a04_rgx_fw_u32(out.isp.quad+64)==(temp+3)/4);
  assert(a04_rgx_fw_u32(out.isp.quad+80)==0x4000);
  assert(out.isp.raw60==2&&out.isp.raw68==0x10000&&!out.isp.raw70&&!out.isp.raw78);
  // Partial BG raw00=200 is not ORed into any native task/ISP field.
  // No source1 sampler/texture/default-USC certification is manufactured.
  for(auto &d:data)for(auto byte:d)assert(byte==0xa5);
  for(unsigned n=0;n<4;n++)assert(!r.control[n]->ui32WriteOffset&&!r.control[n]->ui32ReadOffset);
 }
 // Coherent canonical background DATA component, distinct from flat shader:
 // same four heaps/target, shared at general+8192; DMA PDS+256/+272 avoids
 // EOT0, solid64, immediate shared128, primary192. No native152 USC asset
 // is supplied or invented; this remains a partial source plan.
 a04_rgx_native22_state_input bg={};for(unsigned n=0;n<4;n++)bg.heaps[n]=in.heaps[n];
 bg.target=in.target;bg.shared={in.heaps[0].va+8192,36};
 bg.pds_data={in.heaps[1].va+256,16};bg.pds_code={in.heaps[1].va+272,16};
 a04_rgx_native22_state_image background;assert(!a04_rgx_native22_state_build(&background,&bg));
 in.pds_temps=A04_RGX_NATIVE22_PDS_TEMPS;assert(!compile(&out,in));
 assert(a04_rgx_fw_u32(out.isp.quad+64)==1&&background.target_reference.va==out.isp.target_reference.va);
 assert(A04_RGX_NATIVE22_USC_TEMPS==4&&A04_RGX_PRIMARY_NOP_TEMPS==0);
 for(auto &d:data)for(auto byte:d)assert(byte==0xa5);
 const auto saved=out;
 for(unsigned region=0;region<4;region++){
  auto table=(uint64_t *)pt[region];unsigned at=(in.heaps[region].va>>12)&511;uint64_t old=table[at];assert(old);table[at]=0;
  assert(compile(&out,in)&&!memcmp(&out,&saved,sizeof(out)));table[at]=old;
 }
 in.pds_temps=61;assert(compile(&out,in)&&!memcmp(&out,&saved,sizeof(out)));in.pds_temps=0;
 in.fragment_pds={in.heaps[1].va+65520,64};assert(compile(&out,in)&&!memcmp(&out,&saved,sizeof(out)));
 in.fragment_pds={in.heaps[1].va+192,64};in.fragment_usc={in.heaps[2].va+65532,8};
 assert(compile(&out,in)&&!memcmp(&out,&saved,sizeof(out)));
 printf("PASS %u source program/4heap ISP bindings; distinct primary; no208 or GPU execution\n",cases);
}
'''
class ISPPrograms(unittest.TestCase):
    def test_real_builders_primary_binding_carry_and_atomic_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.cpp').write_text(CODE)
            subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),str(p/'case.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            # Compile the real cross-unit calls on ARM64 too, with no stdio.
            arm=CODE[CODE.index('#include "A04BR3RogueSolidPrograms.h"'):CODE.index('static void setup()')]
            arm=arm[:arm.index('alignas(4096)')]+'''struct combined {a04_rgx_solid_program_image programs;a04_rgx_fragment_kick primary;a04_rgx_isp_fill_image isp;};
int bind(combined *out,const a04_rgx_render_resources *r,const a04_rgx_isp_fill_input *in){combined made={};
if(a04_rgx_solid_program_build(&made.programs,sizeof(made.programs),r,&in->heaps[1],&in->heaps[2],1)||a04_rgx_fragment_kick_build(&made.primary,sizeof(made.primary),96,8,0,0)||a04_rgx_isp_fill_build(&made.isp,in))return 1;*out=made;return 0;}
'''
            (p/'arm.cpp').write_text(arm)
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)
if __name__=='__main__':unittest.main()
