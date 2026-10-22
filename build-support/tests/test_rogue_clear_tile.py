# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Offline native32x32 clear lowering; host layout oracle is NOT GPU execution."""
from pathlib import Path
import subprocess,tempfile,unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
CASE=r'''
#include <assert.h>
#include <string.h>
#include <initializer_list>
#include "A04BR3RogueClearTile.h"
alignas(4096) static uint8_t pc[4096],pd[3][4096],pt[3][4096],data[3][65536],bank[65536];
static a04_rgx_render_resources resource;static uint8_t originalBank[65536],reference[65536];
static uint64_t load64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(){
 a04_rgx_bif heaps[3];const uint64_t va[]={0x65c0000000ULL,0xa800000000ULL,0xba00000000ULL};
 for(unsigned i=0;i<3;i++)heaps[i]={{pc,0x80000000,4096},{pd[i],0x80001000+i*8192,4096},
  {pt[i],0x80002000+i*8192,4096},{data[i],0x81000000+i*65536,65536},va[i],65536,i==2};
 a04_rgx_render_buffer b={bank,0x82000000,65536};
 assert(!a04_rgx_render_build(&resource,&b,&heaps[0],1));assert(!a04_rgx_heap_map_build(heaps,3));
 memcpy(originalBank,bank,sizeof(bank));
 a04_rgx_tq_clear baseline;assert(!a04_rgx_tq_clear_build(&baseline,&resource,&heaps[1],&heaps[2]));
 a04_rgx_clear_tile_request q={0x22,32,32,32,0,0,{0,0,0,0}};
 a04_rgx_clear_tile tile;
 assert(!a04_rgx_clear_tile_build(&tile,&resource,&heaps[1],&heaps[2],&q));
 assert(!memcmp(&tile.task,&baseline,sizeof(baseline)) && tile.first_byte==0 && tile.end_byte==4096 && tile.row_bytes==128);
 // Independent source ledger vectors from original FUN13e2e4. Same one-RT
 // native format22 path, only even stride source field changes (no new ISA).
 const uint64_t csr[]={0x0200003c68800000ULL,0x0200007c68800000ULL,0x020000fc68800000ULL,0x020001fc68800000ULL};
 unsigned checked=0;
 for(unsigned pitch:{32u,64u,128u,256u})for(unsigned height:{32u,64u,128u,256u,512u}){
  if((uint64_t)pitch*4*height>65536)continue;
  for(unsigned width=32;width<=pitch;width+=32){
   q.width=width;q.height=height;q.pitch_pixels=pitch;
   for(unsigned y=0;y+32<=height;y+=32)for(unsigned x=0;x+32<=width;x+=32){
    q.x=x;q.y=y;assert(!a04_rgx_clear_tile_build(&tile,&resource,&heaps[1],&heaps[2],&q));checked++;
    unsigned index=pitch==32?0:pitch==64?1:pitch==128?2:3;
    auto wire=(const uint8_t *)&tile.task.payload;auto old=(const uint8_t *)&baseline.payload;
    assert(load64(wire+0x80)==csr[index]);
    // Every other native208 byte is identical, not an arbitrary caller task.
    for(unsigned i=0;i<208;i++)if(i<0x80 || i>=0x88)assert(wire[i]==old[i]);
    assert(!memcmp(tile.task.usc,baseline.usc,sizeof(tile.task.usc)));
    uint64_t address=resource.bif.va+(uint64_t)y*pitch*4+(uint64_t)x*4;
    assert(load64(tile.task.pixel.image+8)==((address>>2)|((uint64_t)(1u<<19)<<32)));
    for(unsigned i=0;i<64;i++)if(i<8 || i>=16)assert(tile.task.pixel.image[i]==baseline.pixel.image[i]);
    assert(tile.first_byte==(y*pitch+x)*4 && tile.row_bytes==pitch*4);
    assert(tile.end_byte==((y+31)*pitch+x+32)*4 && tile.end_byte<=65536);
    // Host-only reference footprint: distinguish row padding and other tiles
    // from1024 clear pixels; no fake firmware event/UFO or real GPU claim.
    memset(reference,0xa5,sizeof(reference));
    for(unsigned row=0;row<32;row++)memset(reference+tile.first_byte+row*tile.row_bytes,0,128);
    unsigned cleared=0;
    for(unsigned py=0;py<height;py++)for(unsigned px=0;px<pitch;px++){
     bool inside=px>=x && px<x+32 && py>=y && py<y+32;
     const uint8_t *pixel=reference+4*(py*pitch+px);
     for(unsigned c=0;c<4;c++)assert(pixel[c]==(inside?0:0xa5));
     cleared+=inside;
    }
    assert(cleared==1024);
   }
  }
 }
 assert(checked>=100);
 // No emitted task mutates owned GPU memory, programs, CCB, UFO or tables.
 assert(!memcmp(originalBank,bank,sizeof(bank)));
 for(unsigned h=0;h<3;h++)for(unsigned n=0;n<65536;n++)assert(!data[h][n]);
 q={0x22,64,64,64,32,32,{0,0,0,0}};
 memset(&tile,0x6c,sizeof(tile));auto preserved=tile;
 for(unsigned bad=0;bad<13;bad++){
  auto invalid=q;
  if(bad==0)invalid.native_format=0x42475241; // No BGRA/native22 equivalence asserted.
  if(bad==1)invalid.width=0;
  if(bad==2)invalid.height=31;
  if(bad==3)invalid.pitch_pixels=32;
  if(bad==4)invalid.pitch_pixels=65;
  if(bad==5)invalid.x=1;
  if(bad==6)invalid.y=1;
  if(bad==7)invalid.x=64;
  if(bad==8)invalid.y=64;
  if(bad==9)invalid.colour[0]=1;
  if(bad==10){invalid.width=800;invalid.height=1280;invalid.pitch_pixels=832;invalid.x=invalid.y=0;}
  if(bad==11){invalid.width=32;invalid.height=UINT32_MAX;invalid.pitch_pixels=UINT32_MAX;}
  if(bad==12)invalid.colour[3]=UINT32_MAX;
  assert(a04_rgx_clear_tile_build(&tile,&resource,&heaps[1],&heaps[2],&invalid));
  assert(!memcmp(&tile,&preserved,sizeof(tile)));
 }
 assert(a04_rgx_clear_tile_build((a04_rgx_clear_tile *)data[0],&resource,&heaps[1],&heaps[2],&q));
 assert(a04_rgx_clear_tile_build(&tile,&resource,&heaps[1],&heaps[2],(a04_rgx_clear_tile_request *)data[0]));
 auto invalid=heaps[1];invalid.va=0xda00000000ULL;
 assert(a04_rgx_clear_tile_build(&tile,&resource,&invalid,&heaps[2],&q) && !memcmp(&tile,&preserved,sizeof(tile)));
 auto copied=resource;copied.bif.map_bytes=4096;
 assert(a04_rgx_clear_tile_build(&tile,&copied,&heaps[1],&heaps[2],&q) && !memcmp(&tile,&preserved,sizeof(tile)));
 union alignas(8) {a04_rgx_clear_tile output;a04_rgx_clear_tile_request request;} alias;
 alias.request=q;uint8_t saved[sizeof(alias)];memcpy(saved,&alias,sizeof(alias));
 assert(a04_rgx_clear_tile_build(&alias.output,&resource,&heaps[1],&heaps[2],&alias.request));
 assert(!memcmp(&alias,saved,sizeof(alias)));
}
'''
class RogueClearTile(unittest.TestCase):
 def test_original_task_lowering_pitch_address_and_owned_footprint(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'case.cpp').write_text(CASE)
   subprocess.run(['c++','-std=c++14','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(GPU),str(p/'case.cpp'),'-o',str(p/'case')],check=True)
   subprocess.run([str(p/'case')],check=True)
   (p/'arm.cpp').write_text('#include "A04BR3RogueClearTile.h"\nint encode(a04_rgx_clear_tile *o,const a04_rgx_render_resources *r,const a04_rgx_bif *p,const a04_rgx_bif *u,const a04_rgx_clear_tile_request *q){return a04_rgx_clear_tile_build(o,r,p,u,q);}\n')
   subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I',str(GPU),'-c',str(p/'arm.cpp'),'-o',str(p/'arm.o')],check=True)
if __name__=='__main__':unittest.main()
