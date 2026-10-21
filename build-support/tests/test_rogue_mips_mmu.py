# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess,tempfile,unittest
class MipsMMU(unittest.TestCase):
 def test_pte_permissions_bounds_and_atomic_overlap_failure(self):
  inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
  source=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "A04BR3RogueMipsMMU.h"
static uint32_t t[8192],saved[8192];
int main(void){
 assert(!a04_rgx_mips_map(t,8192,0,0x80000000,8192,0,1));
 assert(t[0]==0x02000013&&t[1]==0x02000053);
 assert(!a04_rgx_mips_map(t,8192,8192,0xfffff000,4096,1,0));
 assert(t[2]==0x43ffffd7);
 memcpy(saved,t,sizeof(t));
 assert(a04_rgx_mips_map(t,8192,0,0x90000000,4096,1,0)==3);
 assert(!memcmp(t,saved,sizeof(t)));
 memset(t,0,sizeof(t));t[1]=3;memcpy(saved,t,sizeof(t));
 assert(a04_rgx_mips_map(t,8192,0,0x90000000,8192,1,0)==3);assert(!memcmp(t,saved,sizeof(t)));
 assert(a04_rgx_mips_map(t,8192,0,0x100000000ULL,4096,0,1)==1);
 assert(a04_rgx_mips_map(t,8192,0,0xfffff000,8192,0,1)==1);
 assert(a04_rgx_mips_map(t,8192,1,0x80000000,4096,0,1)==1);
 assert(a04_rgx_mips_map(t,8192,0x2000000-4096,0x80000000,8192,0,1)==2);
 assert(a04_rgx_mips_map(t,8193,0,0x80000000,4096,0,1)==1);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined,address','-I',str(inc),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
