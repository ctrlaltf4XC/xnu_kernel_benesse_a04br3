# SPDX-License-Identifier: MIT
"""Execute actual descriptor builder and DMA command/error paths with hardware mocks."""
from pathlib import Path
import unittest
from test_speed_paths import run_c
ROOT=Path(__file__).resolve().parents[2]
class DmaRead(unittest.TestCase):
 def test_descriptor_and_transfer_contract(self):
  s=(ROOT/'bsd/dev/a04br3_msdc.c').read_text()
  body=s[s.index('static int dma_read('):s.index('/* CMD23 limits CMD18')]
  run_c(r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#define PAGE_SIZE 16384
#define BLOCKS 0x50
#define FIFO 0x14
#define INTR 0x0c
#define CFG 0
#define STS 0x3c
#define DATA_DONE (1u<<12)
#define DATA_ERR ((1u<<14)|(1u<<15))
typedef uintptr_t vm_offset_t;
'''+f'#include "{ROOT}/bsd/dev/a04br3_msdc_dma.h"\n'+r'''
static uint8_t area[PAGE_SIZE+65536] __attribute__((aligned(PAGE_SIZE)));
static vm_offset_t dma_area=(uintptr_t)area;
static uint64_t dma_pa=0x60000000;
static unsigned count,calls,ops[8],started,stopped,fault;
static uint64_t tick;
static uint32_t regs[64];
static void dma_cache_sync(vm_offset_t a,unsigned b){assert(a>=dma_area && a+b<=dma_area+sizeof(area));}
static void panic(const char *m){(void)m;abort();}
static uint64_t deadline(void){return tick+100;}
static uint64_t mach_absolute_time(void){return ++tick;}
static uint32_t rd(unsigned r){
 if(r==INTR)return fault==1?DATA_ERR:fault==2?0:fault==3?(1u<<17):DATA_DONE;
 return regs[r/4];
}
static void wr(unsigned r,uint32_t v){
 regs[r/4]=v;
 if(r==BLOCKS)count=v;
 if(r==0x98 && (v&1u) && !(v&2u)){
  started++;for(unsigned i=0;i<count*512;i++)area[PAGE_SIZE+i]=(uint8_t)i;
 }
 if(r==0x98 && (v&2u))stopped++;
}
static int clear_wait(unsigned r,uint32_t m){(void)r;(void)m;return 0;}
static int command(unsigned op,uint32_t a,int mode,uint32_t *r){
 (void)a;(void)mode;assert(calls<8);ops[calls++]=op;*r=0;return fault==4?EIO:0;
}
'''+body+r'''
static void reset(void){memset(regs,0,sizeof(regs));calls=started=stopped=0;tick=0;fault=0;}
int main(void){
 uint8_t dest[65538];
 for(unsigned n=1;n<=128;n*=2){
  reset();memset(dest,0xaa,sizeof(dest));
  assert(dma_read(100,dest+1,n)==0);
  assert(started==1 && stopped && calls==(n>1?2u:1u));
  assert(ops[calls-1]==(n>1?18u:17u));
  for(unsigned i=0;i<n*512;i++)assert(dest[i+1]==(uint8_t)i);
  assert(dest[0]==0xaa && dest[n*512+1]==0xaa);
  assert(regs[CFG/4]&8u);
  struct a04br3_dma_gpd *g=(void *)area;
  struct a04br3_dma_bd *bd=(void *)(area+64);
  assert(g[0].pointer==dma_pa+64 && g[0].next==dma_pa+28);
  assert(bd->pointer==dma_pa+PAGE_SIZE && bd->length==n*512);
  assert(a04br3_dma_checksum(g)==0 && a04br3_dma_checksum(g+1)==0 && a04br3_dma_checksum(bd)==0);
 }
 for(unsigned f=1;f<=3;f++){
  reset();fault=f;memset(dest,0xaa,sizeof(dest));
  assert(dma_read(100,dest+1,4)==(f==2?ETIMEDOUT:EIO));
  assert(stopped && ops[calls-1]==12 && dest[1]==0xaa && (regs[CFG/4]&8));
 }
 reset();fault=4;assert(dma_read(100,dest,1)==EIO && started==0 && stopped);
 reset();assert(dma_read(100,dest,0)==EINVAL && calls==0);
 assert(dma_read(100,dest,129)==EINVAL && calls==0);
 dma_pa=0x100000000ULL;assert(dma_read(100,dest,1)==EINVAL && calls==0);
 dma_pa=0x60000001;assert(dma_read(100,dest,1)==EINVAL && calls==0);
 return 0;
}
''')
