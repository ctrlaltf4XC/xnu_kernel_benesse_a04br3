# SPDX-License-Identifier: MIT
"""Execute real burst-write and changed-pixel code with bounded hardware mocks."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]

def run_c(source):
 with tempfile.TemporaryDirectory() as d:
  src=Path(d)/'check.c';exe=Path(d)/'check';src.write_text(source)
  subprocess.run(['cc','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)

class SpeedPaths(unittest.TestCase):
 def test_changed_pixels(self):
  header=ROOT/'iokit/Kernel/arm/A04BR3DisplayCopy.h'
  run_c(r'''
#include <stdint.h>
#include <assert.h>
'''+f'#include "{header}"\n'+r'''
int main(void) {
 uint32_t src[128],shadow[128]={0},guard[130];
 for(unsigned i=0;i<130;i++)guard[i]=0x12345678;
 for(unsigned i=0;i<128;i++)src[i]=i;
 assert(a04br3_copy_changed_pixels(guard+1,shadow,src,128)==128);
 for(unsigned i=0;i<128;i++)assert(guard[i+1]==(i|0xff000000u));
 assert(a04br3_copy_changed_pixels(guard+1,shadow,src,128)==0);
 src[63]=0x00000080;
 assert(a04br3_copy_changed_pixels(guard+1,shadow,src,128)==1);
 assert(guard[64]==0xff000080u);
 /* Alpha-only changes produce the same opaque output and no Device store. */
 src[63]=0xaa000080;
 assert(a04br3_copy_changed_pixels(guard+1,shadow,src,128)==0);
 assert(a04br3_copy_changed_pixels(guard+1,shadow,src,0)==0);
 assert(guard[0]==0x12345678 && guard[129]==0x12345678);
 return 0;
}
''')

 def test_burst_write_and_bounds(self):
  s=(ROOT/'bsd/dev/a04br3_msdc.c').read_text()
  body=s[s.index('static int write_blocks_raw('):s.index('static uint32_t le32')]
  body=body.replace('*(volatile uint32_t *)(msdc+TX)=word;', 'test_tx(word);')
  body=body.replace('__asm__ volatile("dsb sy" ::: "memory");','__asm__ volatile("" ::: "memory");')
  gate=s[s.index('static int write_root_blocks('):s.index('static int flush_card(')]
  run_c(r'''
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define BLOCKS 0x50
#define FIFO 0x14
#define INTR 0x0c
#define DATA_DONE (1u<<12)
#define DATA_ERR ((1u<<14)|(1u<<15))
static unsigned blocks,position,calls,ops[8],args[8],modes[8],fault;
static uint64_t tick;
static unsigned write_enabled=1;
static uint32_t root_first=50,root_sectors=256,disk_sectors=500;
static uint64_t deadline(void) {return tick+10000;}
static uint64_t mach_absolute_time(void) {return ++tick;}
static void test_tx(uint32_t word) {assert(word==position/4);position+=4;assert(position<=blocks*512);}
static void wr(unsigned r,uint32_t v) {if(r==BLOCKS){blocks=v;position=0;}}
static int clear_wait(unsigned r,uint32_t m) {(void)r;(void)m;return 0;}
static uint32_t rd(unsigned r) {
 if(r==INTR) return fault==1 ? DATA_ERR : (fault!=2 && position==blocks*512 ? DATA_DONE : 0);
 return 0;
}
static int command(unsigned op,uint32_t arg,int mode,uint32_t *resp) {
 assert(calls<8);ops[calls]=op;args[calls]=arg;modes[calls++]=mode;
 *resp=op==13 ? 0x900 : 0;return 0;
}
'''+body+gate+r'''
int main(void) {
 uint32_t data[16384];for(unsigned i=0;i<16384;i++)data[i]=i;
 for(unsigned n=1;n<=128;n*=2) {
  calls=0;tick=0;
  assert(write_root_blocks(0,(const uint8_t *)data,n)==0);
  assert(position==n*512 && calls==(n==1 ? 2u:3u));
  if(n==1)assert(ops[0]==24 && args[0]==50 && modes[0]==2);
  else assert(ops[0]==23 && args[0]==n && ops[1]==25 && args[1]==50 && modes[1]==4);
  assert(ops[calls-1]==13);
 }
 calls=0;tick=0;assert(write_block_raw(50,(const uint8_t *)data)==0);
 calls=0;fault=1;assert(write_root_blocks(0,(const uint8_t *)data,8)==EIO);
 assert(calls==3 && ops[2]==12);
 calls=0;tick=0;fault=2;assert(write_root_blocks(0,(const uint8_t *)data,8)==ETIMEDOUT);
 assert(calls==3 && ops[2]==12);fault=0;
 calls=0;write_enabled=0;assert(write_root_blocks(0,(const uint8_t *)data,8)==EROFS && !calls);
 write_enabled=1;
 assert(write_root_blocks(255,(const uint8_t *)data,2)==EINVAL && !calls);
 assert(write_root_blocks(UINT64_MAX,(const uint8_t *)data,1)==EINVAL && !calls);
 assert(write_root_blocks(0,(const uint8_t *)data,129)==EINVAL && !calls);
 assert(write_root_blocks(0,(const uint8_t *)data,0)==EINVAL && !calls);
 root_first=499;assert(write_root_blocks(0,(const uint8_t *)data,2)==EINVAL && !calls);
 for(unsigned i=0;i<16384;i++)assert(data[i]==i);
 return 0;
}
''')
