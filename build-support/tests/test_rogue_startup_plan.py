# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

KERNEL=Path(__file__).resolve().parents[2]
HEADER=KERNEL/'iokit/Kernel/arm/rogue'
DDK=KERNEL.parent.parent.parent/'port/gsi/kernel-4.14/drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693'


class StartupPlan(unittest.TestCase):
    def test_transcript_prerequisites_ranges_and_observation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'test.c'
            (root/'img_types.h').write_text('/* scalar literal macro supplied by test */\n')
            (root/'img_defs.h').write_text('/* no runtime DDK declarations used */\n')
            # Compare the actual emitted transcript against DDK register names,
            # values and masks, not a mock MMIO executor that could touch HW.
            src.write_text(r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueStartupPlan.h"
#define IMG_EXPLICIT_INCLUDE_HWDEFS
#define IMG_UINT64_C(x) UINT64_C(x)
#include "rgx_cr_defs_km.h"
#define W32 A04_START_WRITE32
#define W64 A04_START_WRITE64
#define F32 A04_START_FENCE32
#define F64 A04_START_FENCE64
#define R32 A04_START_RMW32
#define R64 A04_START_RMW64
int main(void) {
 struct a04_rgx_start_input x;
 memset(&x,0,sizeof(x));
 x.bvnc=0x001600280036001eULL;x.code_pa=0x80000000;x.data_pa=0x81000000;
 x.table_pa=0x82000000;x.trampoline_pa=0x83000000;x.init_pa=0x84000000;
 x.init_bytes=4096;x.started_offset=128; /* SYNTHETIC ABI offset, never production */
 x.evidence=A04_START_ALL;x.strict_same_address_ordering=1;
 x.poll_budget=10;x.poll_delay_us=3;
 struct a04_rgx_start_plan plan,saved;
 assert(a04_rgx_start_plan(&x,&plan)==A04_START_OK);
 assert(plan.execution_allowed && !plan.missing && plan.count==28);
 uint64_t dusts=RGX_CR_SOFT_RESET_RASCAL_CORE_EN |
   RGX_CR_SOFT_RESET_DUST_A_CORE_EN|RGX_CR_SOFT_RESET_DUST_B_CORE_EN|
   RGX_CR_SOFT_RESET_DUST_C_CORE_EN|RGX_CR_SOFT_RESET_DUST_D_CORE_EN|
   RGX_CR_SOFT_RESET_DUST_E_CORE_EN|RGX_CR_SOFT_RESET_DUST_F_CORE_EN|
   RGX_CR_SOFT_RESET_DUST_G_CORE_EN|RGX_CR_SOFT_RESET_DUST_H_CORE_EN;
 uint64_t bypass=RGX_CR_SLC_CTRL_BYPASS_REQ_IPF_OBJ_EN|
   RGX_CR_SLC_CTRL_BYPASS_REQ_IPF_CPF_EN|RGX_CR_SLC_CTRL_BYPASS_REQ_TPU_EN;
 uint64_t axi=(3ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT)|
  (3ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT)|
  (2ULL<<RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT);
 const struct a04_rgx_start_op expected[]={
  {W32,RGX_CR_SYS_BUS_SECURE,0,0},{F32,RGX_CR_SYS_BUS_SECURE,0,0},
  {W64,RGX_CR_SOFT_RESET,RGX_CR_SOFT_RESET_MASKFULL,0},
  {W64,RGX_CR_SOFT_RESET,RGX_CR_SOFT_RESET_MASKFULL^dusts,0},
  {F64,RGX_CR_SOFT_RESET,0,0},{W64,RGX_CR_SOFT_RESET,RGX_CR_SOFT_RESET_GARTEN_EN,0},
  {R64,RGX_CR_SLC_CTRL_BYPASS,bypass,~bypass},
  {R32,RGX_CR_SLC_CTRL_MISC,RGX_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1|
     RGX_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN,RGX_CR_SLC_CTRL_MISC_ENABLE_PSG_HAZARD_CHECK_EN},
  {W64,RGX_CR_MIPS_WRAPPER_CONFIG,(0x13000000U>>16)|RGX_CR_MIPS_WRAPPER_CONFIG_BOOT_ISA_MODE_MICROMIPS,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP1_CONFIG1,0x1fc00001,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP1_CONFIG2,0x8001800c,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP5_CONFIG1,1,0},{W64,RGX_CR_MIPS_ADDR_REMAP5_CONFIG2,0x8001800c,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP2_CONFIG1,0x1fc01001,0},{W64,RGX_CR_MIPS_ADDR_REMAP2_CONFIG2,0x8100300c,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP3_CONFIG1,0x1fc02001,0},{W64,RGX_CR_MIPS_ADDR_REMAP3_CONFIG2,0x8001700c,0},
  {W64,RGX_CR_MIPS_ADDR_REMAP4_CONFIG1,0x83000001,0},{W64,RGX_CR_MIPS_ADDR_REMAP4_CONFIG2,0x1fc0000e,0},
  {W64,RGX_CR_MTS_GARTEN_WRAPPER_CONFIG,RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META,0},
  {W32,RGX_CR_MIPS_DEBUG_CONFIG,0,0},{W64,RGX_CR_AXI_ACE_LITE_CONFIGURATION,axi,0},
  {A04_START_WAIT_US,0,3,32},{W64,RGX_CR_SOFT_RESET,0,0},{F64,RGX_CR_SOFT_RESET,0,0},
  {A04_START_WAIT_US,0,3,32},{A04_START_POLL_SHARED32,0x84000080,1,0xffffffffU},
  {A04_START_READ_IRQ,RGX_CR_MIPS_WRAPPER_IRQ_STATUS,0,RGX_CR_MIPS_WRAPPER_IRQ_STATUS_EVENT_EN}
 };
 assert(plan.count==sizeof(expected)/sizeof(expected[0]));
 for(unsigned i=0;i<plan.count;i++) {
  assert(plan.ops[i].kind==expected[i].kind && plan.ops[i].address==expected[i].address);
  assert(plan.ops[i].value==expected[i].value && plan.ops[i].keep_mask==expected[i].keep_mask);
 }
 /* Every prerequisite individually blocks the release, even with valid PAs. */
 for(unsigned bit=1;bit<=A04_START_REG_APERTURE;bit<<=1) {
  x.evidence=A04_START_ALL^bit;
  assert(a04_rgx_start_plan(&x,&plan)==A04_START_INCOMPLETE);
  assert(plan.missing==bit && !plan.execution_allowed);
  assert(plan.ops[plan.count-1].kind==A04_START_BLOCKED);
  for(unsigned i=0;i<plan.count;i++)
   assert(!(plan.ops[i].kind==W64 && plan.ops[i].address==RGX_CR_SOFT_RESET && !plan.ops[i].value));
 }
 x.evidence=A04_START_ALL;
 struct a04_rgx_start_input original=x;
 x.trampoline_pa=0;
 assert(a04_rgx_start_plan(&x,&plan)==A04_START_INCOMPLETE);
 assert(plan.missing&A04_START_TRAMPOLINE);x=original;
 x.init_pa=0;
 assert(a04_rgx_start_plan(&x,&plan)==A04_START_INCOMPLETE);
 assert(plan.missing&A04_START_INIT_ABI);x=original;
 x.ready_initial=1;
 assert(a04_rgx_start_plan(&x,&plan)==A04_START_INCOMPLETE);x=original;
 x.device_pa0_valid=1;assert(!a04_rgx_start_plan(&x,&plan));assert(plan.count==26);
 for(unsigned i=0;i<plan.count;i++)assert(plan.ops[i].address!=RGX_CR_MIPS_ADDR_REMAP5_CONFIG1);
 x=original;
 for(unsigned mode=0;mode<15;mode++) {
  x=original;
  switch(mode) {
   case 0:x.bvnc++;break;case 1:x.code_pa=0xfffff000;break;
   case 2:x.data_pa=x.code_pa+0x1000;break;case 3:x.table_pa+=4096;break;
   case 4:x.trampoline_pa+=4096;break;case 5:x.trampoline_pa=0x1fc00000;break;
   case 6:x.init_pa=0x100000000ULL;break;case 7:x.started_offset=4093;break;
   case 8:x.started_offset=4096;break;case 9:x.evidence=1024;break;
   case 10:x.strict_same_address_ordering=0;break;case 11:x.poll_budget=0;break;
   case 12:x.poll_delay_us=1000001;break;case 13:x.init_bytes=0;break;
   case 14:x.device_pa0_valid=2;break;
  }
  memset(&plan,0xa5,sizeof(plan));saved=plan;
  assert(a04_rgx_start_plan(&x,&plan)==A04_START_BAD_INPUT);
  assert(!memcmp(&saved,&plan,sizeof(plan)));
 }
 assert(a04_rgx_start_plan(NULL,&plan)==A04_START_BAD_INPUT);
 struct a04_rgx_start_watch w={2,1,0,0,0},prior=w;
 assert(a04_rgx_start_sample(&w,1,0,0,0)==A04_START_BAD_INPUT);
 assert(!memcmp(&w,&prior,sizeof(w)));
 assert(a04_rgx_start_sample(&w,0,1,4,1)==A04_START_INCOMPLETE);
 assert(!w.ready && w.irq_pending && w.exception_status==4);
 assert(a04_rgx_start_sample(&w,1,0,0,1)==A04_START_OK && w.ready);
 assert(!w.irq_pending);
 assert(a04_rgx_start_sample(&w,1,1,0,1)==A04_START_OK && w.irq_pending);
 w=prior;w.armed_zero=0;
 assert(a04_rgx_start_sample(&w,1,1,0,1)==A04_START_BAD_INPUT);
 w=prior;
 assert(a04_rgx_start_sample(&w,0xffffffff,1,0,1)==A04_START_INCOMPLETE && !w.ready);
 assert(a04_rgx_start_sample(&w,2,1,0,1)==A04_START_INCOMPLETE && !w.ready);
 assert(a04_rgx_start_sample(&w,1,1,0,1)==A04_START_INCOMPLETE && !w.ready); /* expired */
 assert(a04_rgx_start_irq_clear_value(0)==0);
 assert(a04_rgx_start_irq_clear_value(2)==0);
 assert(a04_rgx_start_irq_clear_value(3)==RGX_CR_MIPS_WRAPPER_IRQ_CLEAR_EVENT_EN);
 return 0;
}
''')
            exe=root/'test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-I',str(HEADER),'-I',str(root),'-I',str(DDK/'hwdefs/km'),
                            str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
            subprocess.run(['c++','-x','c++','-std=c++14','-ffreestanding',
                            '-fno-exceptions','-fno-rtti','-Wall','-Wextra','-Werror',
                            '-I',str(HEADER),'-I',str(root),'-I',str(DDK/'hwdefs/km'),'-c',str(src),
                            '-o',str(root/'test.o')],check=True)

    def test_arm64_header_and_pinned_ddk_policy(self):
        clang=shutil.which('clang++')
        if not clang:self.skipTest('clang++ required for ARM64 compile')
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'compile.cpp'
            src.write_text('''#include "A04BR3RogueStartupPlan.h"
int plan(const a04_rgx_start_input *in,struct a04_rgx_start_plan *out) {
 return a04_rgx_start_plan(in,out);
}
''')
            subprocess.run([clang,'--target=arm64-apple-darwin','-std=c++14',
                            '-ffreestanding','-fno-exceptions','-fno-rtti','-Wall','-Wextra','-Werror',
                            '-O2','-Wframe-larger-than=4096','-I',str(HEADER),'-c',str(src),
                            '-o',str(root/'compile.o')],check=True)
        config=(DDK/'hwdefs/km/configs/rgxconfig_km_22.V.54.30.h').read_text()
        for feature in ('RGX_FEATURE_MIPS','RGX_FEATURE_AXI_ACELITE','RGX_FEATURE_SYS_BUS_SECURE_RESET'):
            self.assertIn('#define '+feature,config)
        self.assertNotIn('#define RGX_FEATURE_S7_TOP_INFRASTRUCTURE',config)
        self.assertNotIn('#define RGX_FEATURE_SLC_VIVT',config)
        self.assertIn('(32U)',config)
        start=(DDK/'services/server/devices/rgx/rgxstartstop.c').read_text()
        self.assertIn('#define SOC_FEATURE_STRICT_SAME_ADDRESS_WRITE_ORDERING',start)
        threshold=(DDK/'hwdefs/km/tpu_cacheability_km.h').read_text()
        self.assertIn('(128*1024)',threshold)
        core=(DDK/'hwdefs/km/cores/rgxcore_km_22.40.54.30.h').read_text()
        for brn in ('60084','61450','63553'): self.assertIn('FIX_HW_BRN_'+brn,core)
