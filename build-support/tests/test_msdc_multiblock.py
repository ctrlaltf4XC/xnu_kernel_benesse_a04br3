# SPDX-License-Identifier: MIT
"""Run the actual PIO read loop against a simulated MSDC FIFO."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class MultiBlock(unittest.TestCase):
    def test_pio(self):
        source=(Path(__file__).resolve().parents[2]/'bsd/dev/a04br3_msdc.c').read_text()
        body=source[source.index('static int read_blocks('):source.index('/* CMD23/CMD25')]
        harness=r"""
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define BLOCKS 0x50
#define FIFO 0x14
#define INTR 0x0c
#define RX 0x1c
#define DATA_DONE (1u<<12)
#define DATA_ERR ((1u<<14)|(1u<<15))
static uintptr_t msdc;
static unsigned count,position,calls,ops[8],args[8],modes[8],fault;
static uint64_t tick;
static uint64_t deadline(void) { return tick+10000; }
static uint64_t mach_absolute_time(void) { return ++tick; }
static void wr(unsigned r,uint32_t v) { if(r==BLOCKS) {count=v;position=0;} }
static int clear_wait(unsigned r,uint32_t v) { (void)r;(void)v;return 0; }
static uint32_t rd(unsigned r) {
 if(r==INTR) return fault ? DATA_ERR : (position==count*512 ? DATA_DONE : 0);
 if(r==FIFO) return position<count*512 ? 128 : 0;
 if(r==RX) { unsigned v=position/4;position+=4;return v; }
 return 0;
}
static int command(unsigned op,uint32_t arg,int mode,uint32_t *resp) {
 assert(calls<8);ops[calls]=op;args[calls]=arg;modes[calls++]=mode;*resp=0;return 0;
}
"""+body+r"""
int main(void) {
 uint32_t buffer[16384+2];
 for(unsigned n=1;n<=128;n*=2) {
  calls=0;tick=0;memset(buffer,0xff,sizeof(buffer));
  assert(read_blocks(17,100,(uint8_t *)(buffer+1),n)==0);
  assert(calls==(n==1 ? 1u:2u));
  if(n>1) assert(ops[0]==23 && args[0]==n && ops[1]==18 && modes[1]==3);
  else assert(ops[0]==17 && modes[0]==1);
  for(unsigned i=0;i<n*128;i++) assert(buffer[i+1]==i);
  assert(buffer[0]==UINT32_MAX && buffer[n*128+1]==UINT32_MAX);
 }
 calls=0;fault=1;assert(read_blocks(17,100,(uint8_t *)buffer,8)==EIO);
 assert(calls==3 && ops[2]==12);
 calls=0;assert(read_blocks(17,100,(uint8_t *)buffer,0)==EINVAL);
 assert(read_blocks(17,100,(uint8_t *)buffer,129)==EINVAL && calls==0);
 calls=0;fault=0;assert(read_block(8,0,(uint8_t *)buffer)==0 && ops[0]==8);
 return 0;
}
"""
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'read.c';exe=Path(d)/'read';src.write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
