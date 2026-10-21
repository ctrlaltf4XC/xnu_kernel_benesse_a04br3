# SPDX-License-Identifier: MIT
"""Old board source arithmetic, not acceptance of an arbitrary208 task."""
from pathlib import Path
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
GPU=HERE.parents[1]/'iokit/Kernel/arm/rogue'
CODE=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "A04BR3RogueTransferBackground.h"
static uint32_t r32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void put32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
// Native FUN_00129f88 recovered expressions with its original P offsets.
// Context PDS base is A800000000; deliberately compute LOW32 differences as
// original does, independently of this API's supplied heap-relative offsets.
static void native(uint64_t ref[4],const struct a04_rgx_tq_background_input *in){
 uint8_t p[0x1a8]={0},ctx[0x5520]={0};uint64_t base=UINT64_C(0xa800000000);
 put32(ctx+0x5518,(uint32_t)base);
 put32(p+0x160,(uint32_t)(base+in->task_offset));
 put32(p+0x178,(uint32_t)(base+in->secondary_code));
 put32(p+0x170,(uint32_t)(base+in->second_data));
 put32(p+0x168,(uint32_t)(base+in->first_data));
 put32(p+0x180,in->first_words);put32(p+0x184,in->second_words);
 put32(p+0x18c,in->shared_words);put32(p+0x1a4,in->pds_temps);
 ref[0]=((uint64_t)(uint32_t)(r32(p+0x160)-r32(ctx+0x5518))&0xfffffff0) |
        ((uint64_t)((uint32_t)(r32(p+0x178)-r32(ctx+0x5518))>>4)<<36);
 ref[1]=((uint64_t)(uint32_t)(r32(p+0x170)-r32(ctx+0x5518))&0xfffffff0)<<32;
 ref[2]=(uint64_t)(uint32_t)(r32(p+0x168)-r32(ctx+0x5518))&0xfffffff0;
 ref[3]=((uint64_t)r32(p+0x180)&0x7fc)<<21 |
        (uint64_t)((r32(p+0x18c)+15u)>>4)<<55 |
        (uint64_t)((r32(p+0x1a4)+3u)>>2)&0xf |
        ((uint64_t)(r32(p+0x184)>>2)&0x7f)<<16;
}
static struct a04_rgx_tq_background_input good(void){
 struct a04_rgx_tq_background_input in={UINT64_C(0x100000000),0x100,64,0x200,16,0x300,8192,0x4000,2048,4,4,4,1};return in;
}
static void reject(struct a04_rgx_tq_background_input in){
 struct a04_rgx_tq_background_fields out,saved;memset(&out,0xa5,sizeof(out));saved=out;
 assert(a04_rgx_tq_background_encode(&out,sizeof(out),&in,sizeof(in)));
 assert(!memcmp(&out,&saved,sizeof(out)));
}
int main(void){
 unsigned cases=0;uint32_t rng=89203;
 for(unsigned shared=1;shared<=256;shared++)for(unsigned temps=1;temps<=60;temps++){
  struct a04_rgx_tq_background_input in=good();uint64_t ref[4];
  rng=rng*1664525u+1013904223u;
  in.task_offset=rng&0xffff0000;in.secondary_code=(rng^0xabcd0000)&0xffff0000;
  in.first_data=(rng^0x12340000)&0xffff0000;in.second_data=(rng^0x56780000)&0xffff0000;
  in.first_words=4*((rng%511)+1);in.second_words=4*((rng%127)+1);
  in.shared_words=shared;in.pds_temps=temps;
  struct a04_rgx_tq_background_fields got;
  assert(!a04_rgx_tq_background_encode(&got,sizeof(got),&in,sizeof(in)));
  native(ref,&in);assert(got.raw38==ref[0]&&got.raw40==ref[1]&&got.raw48==ref[2]&&got.raw50==ref[3]);assert(got.shader_branch_raw00==0x200&&!got.reserved);++cases;
 }
 struct a04_rgx_tq_background_input in=good();
 #define BAD(field,value) do{in=good();in.field=(value);reject(in);}while(0)
 BAD(heap_bytes,0);BAD(heap_bytes,UINT64_C(0x100000001));
 BAD(task_offset,UINT64_MAX);BAD(task_offset,0xfffffff0);BAD(task_bytes,0);BAD(task_bytes,32);BAD(task_bytes,37);
 BAD(secondary_code,UINT64_C(0x100000000));BAD(secondary_code_bytes,1);BAD(secondary_code_bytes,0);
 BAD(first_words,0);BAD(first_words,3);BAD(first_words,2048);BAD(first_words,UINT32_MAX);
 BAD(second_words,0);BAD(second_words,2);BAD(second_words,512);
 BAD(first_data_bytes,15);BAD(second_data_bytes,15);BAD(first_data_bytes,UINT64_MAX);
 BAD(shared_words,0);BAD(shared_words,257);BAD(pds_temps,0);BAD(pds_temps,61);
 for(unsigned a=1;a<16;a++){BAD(task_offset,a);BAD(secondary_code,a);BAD(first_data,a);BAD(second_data,a);}
 struct a04_rgx_tq_background_fields out,saved;memset(&out,0xa5,sizeof(out));saved=out;in=good();
 for(size_t n=0;n<sizeof(out);n++)assert(a04_rgx_tq_background_encode(&out,n,&in,sizeof(in)));
 for(size_t n=0;n<sizeof(in);n++)assert(a04_rgx_tq_background_encode(&out,sizeof(out),&in,n));
 assert(a04_rgx_tq_background_encode(NULL,sizeof(out),&in,sizeof(in)));
 assert(a04_rgx_tq_background_encode(&out,sizeof(out),NULL,sizeof(in)));
 assert(a04_rgx_tq_background_encode((void *)(UINTPTR_MAX-7),sizeof(out),&in,sizeof(in)));
 assert(a04_rgx_tq_background_encode(&out,sizeof(out),(void *)(UINTPTR_MAX-7),sizeof(in)));
 assert(a04_rgx_tq_background_encode((void *)((uint8_t *)&out+1),sizeof(out),&in,sizeof(in)));
 assert(a04_rgx_tq_background_encode(&out,sizeof(out),(void *)((uint8_t *)&in+1),sizeof(in)));
 assert(!memcmp(&out,&saved,sizeof(out)));
 struct a04_rgx_tq_background_input savedIn=in;
 assert(a04_rgx_tq_background_encode((void *)&in,sizeof(out),&in,sizeof(in)));
 assert(!memcmp(&in,&savedIn,sizeof(in)));
 struct {uint64_t before;struct a04_rgx_tq_background_fields fields;uint64_t after;} guard;
 memset(&guard,0xa5,sizeof(guard));
 assert(!a04_rgx_tq_background_encode(&guard.fields,sizeof(guard),&in,sizeof(in)));
 assert(guard.before==UINT64_C(0xa5a5a5a5a5a5a5a5)&&guard.after==guard.before);
 // Exact end of4GiB is legal; no high bits disappear in field packing.
 in=good();in.secondary_code=0xfffffff0;in.secondary_code_bytes=16;
 assert(!a04_rgx_tq_background_encode(&out,sizeof(out),&in,sizeof(in)));
 assert((out.raw38>>32)==0xfffffff0);
 printf("PASS %u native old208 background-field configurations + bounds\n",cases);
}
'''
class TransferBackground(unittest.TestCase):
    def test_native_arithmetic_and_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'case.c').write_text(CODE)
            subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-parentheses','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(GPU),str(p/'case.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            caller='#include "A04BR3RogueTransferBackground.h"\nint run(struct a04_rgx_tq_background_fields *o,const struct a04_rgx_tq_background_input *i){return a04_rgx_tq_background_encode(o,sizeof(*o),i,sizeof(*i));}\n'
            (p/'caller.c').write_text(caller)
            subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-I',str(GPU),'-c',str(p/'caller.c'),'-o',str(p/'arm.o')],check=True)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-ffreestanding','-I',str(GPU),'-x','c++','-fsyntax-only',str(p/'caller.c')],check=True)
if __name__=='__main__':unittest.main()
