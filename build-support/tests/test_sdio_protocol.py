# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,unittest
class SDIOProtocol(unittest.TestCase):
 def test_card_selection_cis_and_bounded_failure(self):
  inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/sdio'
  source=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "A04BR3SDIOProtocol.h"
static unsigned calls,delays,fault,selected,ready;
static unsigned char cis[256];
static int cmd(void*p,unsigned op,uint32_t a,unsigned resp,uint32_t*r){(void)p;assert(++calls<400);*r=0;
 if(op==0){assert(!resp&&!a);return 0;}
 if(op==5){assert(resp==3);*r=0x20ff8000;if(a){assert(a==0xff8000);if(fault!=1)*r|=0x80000000;ready++;}return 0;}
 if(op==3){assert(ready);*r=0x12340000;return 0;}
 if(op==7){assert(a==0x12340000&&resp==7);selected=1;return 0;}
 assert(op==52&&resp==1&&selected&&!(a&0x80000000));unsigned reg=a>>9;assert(reg<=0x1ffff);
 if(fault==2){*r=0x100;return 0;}
 if(reg==0)*r=0x32;else if(reg==9)*r=0x80;else if(reg>=0x80)*r=cis[reg-0x80];
 return 0;}
static void delay(void*p,unsigned us){(void)p;assert(us==10000);delays++;}
static void reset(void){calls=delays=selected=ready=0;memset(cis,0xff,sizeof(cis));cis[0]=0x20;cis[1]=4;cis[2]=0x7a;cis[3]=3;cis[4]=0x30;cis[5]=0x66;}
int main(void){struct a04_sdio_io io={0,cmd,delay};struct a04_sdio_card c={0};
 reset();assert(!a04_sdio_identify(&io,&c));assert(c.vendor==0x37a&&c.device==0x6630&&c.functions==2&&c.revision==0x32);
 reset();fault=1;assert(a04_sdio_identify(&io,&c)==5&&ready==100&&delays==99&&!selected);
 reset();fault=2;assert(a04_sdio_identify(&io,&c)==8);
 reset();fault=0;memset(cis,0,sizeof(cis));assert(a04_sdio_identify(&io,&c)==16);
 reset();cis[1]=3;assert(a04_sdio_identify(&io,&c)==14);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=undefined,address','-I',str(inc),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
