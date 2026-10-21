# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess,tempfile,unittest
class SDIOPads(unittest.TestCase):
 def test_command_data_clock_pulls_preserve_other_hosts(self):
  inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/sdio'
  source=r'''
#include <stdint.h>
#include <assert.h>
#include "A04BR3SDIOPads.h"
static uint32_t e40,e50;
static void write_(void*p,unsigned off,uint32_t value){(void)p;
 if(off==0xe44)e40|=value;else if(off==0xe48)e40&=~value;
 else if(off==0xe54)e50|=value;else if(off==0xe58)e50&=~value;else assert(0);}
int main(void){for(unsigned v=0;v<65536;v+=31){e40=v;e50=65535-v;uint32_t a=e40,b=e50;
 a04_sdio_pad_pulls(0,write_);
 assert((e40&0xffff888f)==(a&0xffff888f));assert((e40&0x7770)==0x1110);
 assert((e50&0xfffff888)==(b&0xfffff888));assert((e50&0x777)==0x161);
 }return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-I',str(inc),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
