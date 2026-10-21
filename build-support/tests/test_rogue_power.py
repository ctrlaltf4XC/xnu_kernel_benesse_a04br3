# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class RoguePower(unittest.TestCase):
 def test_order_power_ack_sram_bus_protection_and_timeouts(self):
  inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
  source=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "A04BR3RoguePower.h"
static uint32_t r[4][1024];static unsigned order[4],count,core_writes,delays,fault;
static uint32_t read_(void*p,unsigned b,unsigned o){(void)p;assert(b<4&&o/4<1024);return r[b][o/4];}
static void write_(void*p,unsigned b,unsigned o,uint32_t v){(void)p;assert(b<4&&o/4<1024);
 if(b==A04_RGX_SPM){unsigned bit=o==0x2c4?25:o==0x2c0?24:4;assert(o==0x2c4||o==0x2c0||o==0x214);
  if((v&12)==12 && !(r[b][o/4]&8)){assert(count<3);order[count++]=o;}
  r[b][o/4]=v;
  if((v&12)==12 && fault!=1){r[0][0x60c/4]|=1u<<bit;r[0][0x610/4]|=1u<<bit;}
  if(!(v&0xf00) && fault==2)r[b][o/4]|=0xf000;
 }else if(b==A04_RGX_INFRA){r[b][o/4]=v;if(o==0x220 && fault!=3)r[b][0x228/4]=v;}
 else if(b==A04_RGX_MFG){assert(o==8&&v==15);assert(count==3);assert(order[0]==0x2c4&&order[1]==0x2c0&&order[2]==0x214);assert(!(r[1][0x228/4]&0x24));core_writes++;if(fault!=4)r[b][0]&=~v;}
 else {assert(b==A04_RGX_TOP && ((o==0x80&&v==4)||(o==0xb0&&v==0xc0)));}
}
static void delay_(void*p,unsigned us){(void)p;assert(us==10);assert(++delays<=30000);}
static void reset(void){memset(r,0,sizeof(r));r[0][0x2c0/4]=r[0][0x214/4]=0xf12;r[0][0x2c4/4]=0x12;r[1][0x220/4]=r[1][0x228/4]=0x24;r[3][0]=15;count=core_writes=delays=0;}
int main(void){struct a04_rgx_io io={0,read_,write_,delay_};
 reset();assert(a04_rgx_power_on(&io)==0&&core_writes==1&&count==3);
 reset();fault=1;assert(a04_rgx_power_on(&io)==1&&core_writes==0&&delays==10000);
 reset();fault=2;assert(a04_rgx_power_on(&io)==2&&core_writes==0);
 reset();fault=3;assert(a04_rgx_power_on(&io)==3&&core_writes==0&&count==2);
 reset();fault=4;assert(a04_rgx_power_on(&io)==5&&core_writes==1);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source);subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(inc),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
