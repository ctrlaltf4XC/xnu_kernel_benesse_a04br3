# SPDX-License-Identifier: MIT
"""Execute the production transport/PIO headers against protocol and MMIO fakes."""
from pathlib import Path
import subprocess
import tempfile
import unittest

INCLUDE = Path(__file__).resolve().parents[2] / 'iokit/Kernel/arm/sdio'


class SDIOTransport(unittest.TestCase):
    def run_c(self, source):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.c').write_text(source)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I', str(INCLUDE),
                            str(path / 'test.c'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)
            # These headers also compile in the kernel's C++ dialect.
            subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                            '-I', str(INCLUDE), '-x', 'c++', '-fsyntax-only',
                            str(path / 'test.c')], check=True)

    def test_enable_ownership_chunking_and_errors(self):
        self.run_c(r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "A04BR3SDIOTransport.h"
static unsigned enables, ready_reads, data_calls, delays, own_reads, own_request;
static unsigned timeout_ready, timeout_own, data_error, r5, wrong_chip, command_error;
static uint32_t args[130];
static unsigned char fbr[2];
static int command(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *out){
 (void)p; assert(op==52 && response==1); *out=0;
 if(command_error)return 1;
 if(r5){*out=r5;return 0;}
 unsigned fn=(arg>>28)&7, address=(arg>>9)&0x1ffff;
 assert(!(arg&(1u<<27))); /* RAW is unsafe on write-only side-effect registers. */
 if(!fn){
  if(address==2){if(arg>>31)enables=arg&255;else *out=enables;}
  else if(address==3){ready_reads++;if(!timeout_ready && ready_reads>=3)*out=enables;}
  else {assert(arg>>31);assert(address==0x110 || address==0x111 || address==0x210 || address==0x211);fbr[address&1]=arg&255;}
 }else{assert(fn==2 && (arg>>31));assert((arg&255)==2);
  if(address==5)own_request++;else assert(address==4);
 }
 return 0;
}
static void delay(void *p,unsigned us){(void)p;assert(us==1000);assert(++delays<=200);}
static int transfer(void *p,uint32_t arg,void *buf,unsigned len){
 (void)p;assert(data_calls<130);args[data_calls++]=arg;
 if(data_error)return A04_SDIO_DATA;
 assert((arg&511)==((arg&(1u<<27))?len/512:(len&511)));
 if(!(arg>>31)){
  memset(buf,0,len);
  if(len==4 && ((arg>>28)&7)==2){
   unsigned address=(arg>>9)&0x1ffff;
   uint8_t *b=(uint8_t *)buf;
   if(address==0){b[0]=wrong_chip?0:0x30;b[1]=0x66;b[2]=0xaa;}
   if(address==4){own_reads++;if(own_request && !timeout_own && own_reads>=4)b[1]=1;}
  }
 }
 return 0;
}
static void reset(void){
 enables=1;ready_reads=data_calls=delays=own_reads=own_request=0;
 timeout_ready=timeout_own=data_error=r5=wrong_chip=command_error=0;
 memset(args,0,sizeof(args));memset(fbr,0,sizeof(fbr));
}
int main(void){
 struct a04_sdio_io io={0,command,delay};
 struct a04_sdio_transport t={&io,transfer,2,0,0};
 struct a04_sdio_wmt_state s={0,0,0,0};
 reset();assert(!a04_sdio_wmt_prepare(&t,&s));
 assert(enables==5 && t.enabled==4 && fbr[0]==0 && fbr[1]==2);
 assert(s.ccir==0xaa6630 && !s.chlpcr_before && s.chlpcr_after==0x100 && s.polls==3 && own_request==1);
 assert(!a04_sdio_enable(&t,1));assert(t.enabled==6 && enables==7);
 uint8_t buffer[1536];memset(buffer,0xab,sizeof(buffer));
 data_calls=0;assert(!a04_sdio_transfer(&t,2,0x18,1,0,buffer,1536));assert(data_calls==1);
 assert(args[0]==0xa8003003);
 assert(a04_sdio_transfer(&t,2,0x18,1,0,buffer,1100)==A04_SDIO_ARGUMENT);
 data_calls=0;assert(!a04_sdio_transfer(&t,1,0x100,0,1,buffer,1100));assert(data_calls==3);
 assert(args[0]==0x14020000 && args[1]==0x14060000 && args[2]==0x140a004c);
 data_calls=0;assert(!a04_sdio_write32(&t,2,0xc,0x12345678));assert(data_calls==1);
 unsigned before=data_calls;
 assert(a04_sdio_read32(&t,2,0,0)==A04_SDIO_ARGUMENT);
 assert(a04_sdio_transfer(&t,2,0x1fffe,0,1,buffer,4)==A04_SDIO_ARGUMENT);
 assert(a04_sdio_transfer(&t,2,0,0,0,buffer,65537)==A04_SDIO_ARGUMENT);
 assert(a04_sdio_transfer(&t,2,0,0,0,buffer,0)==A04_SDIO_ARGUMENT);
 assert(a04_sdio_transfer(&t,7,0,0,0,buffer,4)==A04_SDIO_ARGUMENT);
 assert(a04_sdio_enable(&t,3)==A04_SDIO_ARGUMENT && data_calls==before);
 data_error=1;assert(a04_sdio_transfer(&t,2,0x18,1,0,buffer,1536)==A04_SDIO_DATA);
 assert(t.failed==A04_SDIO_DATA && data_calls==before+1);
 assert(a04_sdio_transfer(&t,2,0x18,1,0,buffer,4)==A04_SDIO_POISONED);
 assert(a04_sdio_enable(&t,1)==A04_SDIO_POISONED);
 reset();t.enabled=0;t.failed=0;timeout_ready=1;
 assert(a04_sdio_enable(&t,2)==A04_SDIO_TIMEOUT && ready_reads==100 && delays==99 && !t.enabled);
 reset();timeout_own=1;assert(a04_sdio_wmt_prepare(&t,&s)==A04_SDIO_TIMEOUT);
 assert(s.polls==100 && delays==101);
 reset();wrong_chip=1;assert(a04_sdio_wmt_prepare(&t,&s)==A04_SDIO_CHIP && !own_request);
 const unsigned errors[]={0x100,0x200,0x800,0x4000,0x8000};
 for(unsigned k=0;k<5;k++){reset();r5=errors[k];assert(a04_sdio_enable(&t,2)==A04_SDIO_R5);}
 reset();command_error=1;assert(a04_sdio_enable(&t,2)==A04_SDIO_COMMAND);
 return 0;
}
''')

    def test_real_pio_fifo_and_failure_paths(self):
        self.run_c(r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "A04BR3SDIOPIO.h"
static uint32_t regs[0x300/4];
static uint8_t bytes[65536];
static unsigned pos, total, writing, ticks, mode, byte_ops, issued;
static uint32_t read_reg(void *ctx,unsigned off,unsigned width){
 (void)ctx;
 if(off==0x3c)return mode==1?3:0;
 if(off==0x14){
  if(!issued)return mode==2?(1u<<31):0;
  if(writing)return mode==3?(1u<<16):0;
  if(mode==3)return 0;
  return total-pos>128?128:total-pos;
 }
 if(off==0xc && issued){
  if(mode==4)return 0x200;
  if(mode==5)return 0x400;
  if(mode==6)return 0x8000|0x1000;
  if(mode==7)return 0x4000;
  if(mode==8)return 0;
  return regs[off/4] | ((mode!=9 && (pos==total || mode==10))?0x1000:0);
 }
 if(off==0x40)return mode==11?0x100:0;
 if(off==0x1c){
  assert(!writing && pos+width<=total);uint32_t v=0;
  for(unsigned k=0;k<width;k++)v|=(uint32_t)bytes[pos++]<<(k*8);
  if(width==1)byte_ops++;
  return v;
 }
 assert(width==4);return regs[off/4];
}
static void write_reg(void *ctx,unsigned off,uint32_t v,unsigned width){
 (void)ctx;
 if(off==0xc){regs[off/4]&=~v;return;}
 if(off==0x14){assert(v==(1u<<31));return;}
 if(off==0x18){
  assert(writing && pos+width<=total);
  for(unsigned k=0;k<width;k++)bytes[pos++]=(uint8_t)(v>>(k*8));
  if(width==1)byte_ops++;
  return;
 }
 assert(width==4);regs[off/4]=v;
 if(off==0x34){issued++;regs[0xc/4]=0x100;assert((v&63)==53);}
}
static void delay(void *ctx,unsigned us){(void)ctx;assert(us==10);assert(++ticks<=10000+total*4);}
static void reset(unsigned len,unsigned wr,unsigned fault){
 memset(regs,0,sizeof(regs));memset(bytes,0,sizeof(bytes));
 pos=0;total=len;writing=wr;ticks=0;mode=fault;byte_ops=issued=0;
 regs[0x208/4]=0x80000001;
}
int main(void){
 struct a04_sdio_regs r={0,read_reg,write_reg,delay};uint8_t buffer[65538];
 const unsigned lengths[]={1,2,3,4,127,128,129,255,256,257,511,512,1024,2560,65536};
 for(unsigned wr=0;wr<2;wr++)for(unsigned k=0;k<15;k++){
  unsigned len=lengths[k];reset(len,wr,0);
  memset(buffer,0xcc,sizeof(buffer));
  for(unsigned i=0;i<len;i++){if(wr)buffer[i+1]=(uint8_t)i;else bytes[i]=(uint8_t)i;}
  unsigned blocks=len>512?len/512:1, blksz=len>512?512:len;
  uint32_t arg=(wr?0x80000000u:0)|0x24000000|(len>512?(1u<<27)|blocks:(len&511));
  assert(!a04_sdio_pio(&r,arg,buffer+1,len));assert(pos==len && issued==1);
  assert(buffer[0]==0xcc && buffer[len+1]==0xcc);
  for(unsigned i=0;i<len;i++)assert((wr?bytes[i]:buffer[i+1])==(uint8_t)i);
  assert(regs[0x34/4]==(53|128|((blocks>1?2:1)<<11)|(wr<<13)|(blksz<<16)));
  assert(regs[0x50/4]==blocks && regs[0x38/4]==arg);
  unsigned count=(!wr && blksz>256)?blksz+17:273;
  assert(regs[0x208/4]==(0x80000001|(count<<11)));
 }
 for(unsigned fault=1;fault<=11;fault++){
  reset(7,0,fault);
  int e=a04_sdio_pio(&r,0x24000007,buffer,7);
  if(fault==4 || fault==5)assert(e==A04_SDIO_COMMAND);
  else if(fault==6 || fault==7)assert(e==A04_SDIO_DATA);
  else if(fault==11)assert(e==A04_SDIO_R5);
  else if(fault==10)assert(e==0 && pos==7); /* completion before final FIFO drain */
  else assert(e==A04_SDIO_TIMEOUT && ticks==(fault<=2?10000:10028));
 }
 reset(7,1,10);assert(a04_sdio_pio(&r,0xa0000007,buffer,7)==A04_SDIO_DATA && !pos);
 reset(7,1,3);assert(a04_sdio_pio(&r,0xa0000007,buffer,7)==A04_SDIO_TIMEOUT && !pos);
 reset(4,0,0);assert(a04_sdio_pio(&r,0x2c000004,buffer,4)==A04_SDIO_ARGUMENT && !issued);
 assert(a04_sdio_pio(&r,0x24000003,buffer,4)==A04_SDIO_ARGUMENT && !issued);
 return 0;
}
''')
