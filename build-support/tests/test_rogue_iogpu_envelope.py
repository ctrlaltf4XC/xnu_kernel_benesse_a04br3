# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Observed envelope ONLY; these records contain no executable GPU commands."""
from pathlib import Path
import subprocess
import tempfile
import unittest
GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'

class IOGPUEnvelope(unittest.TestCase):
    def test_byte_bounds_alias_rejection_and_opaque_token_roundtrip(self):
        source=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueIOGPUEnvelope.h"
alignas(8) static unsigned char wire[64*56+16];
alignas(8) static a04_iogpu56_record out[65],saved[65];
alignas(8) static a04_iogpu56_envelope meta;
static void put32(unsigned char *p,uint32_t v){for(unsigned n=0;n<4;n++)p[n]=(unsigned char)(v>>(8*n));}
static void put64(unsigned char *p,uint64_t v){for(unsigned n=0;n<8;n++)p[n]=(unsigned char)(v>>(8*n));}
int main(){
 uint64_t scalar[]={UINT64_MAX,UINT32_MAX,64,56};
 memset(out,0xa5,sizeof(out));
 // Intentionally unaligned wire input; tokens can be non-address values.
 for(unsigned n=0;n<64;n++){
  auto p=wire+1+n*56;put32(p,n);put32(p+4,7+n);put32(p+8,0);put32(p+12,0x12345678);
  put64(p+16,UINT64_MAX-n);put64(p+24,1+n);put32(p+32,0xffffffff);put32(p+36,0xaabbccdd);
  put32(p+40,n+19);put32(p+44,0xffffffff);put64(p+48,0x123456789abcdef0ULL+n);
 }
 assert(!a04_iogpu56_decode(scalar,4,wire+1,64*56,out,64,&meta));
 assert(meta.queue_id==UINT64_MAX && meta.flags==UINT32_MAX && meta.count==64);
 for(unsigned n=0;n<64;n++){
  assert(out[n].command_shmem_id==n && out[n].segments_shmem_id==n+7 && out[n].reserved0c==0x12345678);
  assert(out[n].scheduled_token==UINT64_MAX-n && out[n].completed_token==n+1 && out[n].deadline_namespace==n+19);
  assert(out[n].reserved24==0xaabbccdd && out[n].reserved2c==0xffffffff && out[n].optional20_shmem_id==0xffffffff);
  assert(out[n].deadline==0x123456789abcdef0ULL+n);
 }
 for(unsigned char c: ((unsigned char (&)[56])out[64]))assert(c==0xa5);
 memcpy(saved,out,sizeof(out));auto oldmeta=meta;
 scalar[2]=1;
 for(unsigned size=0;size<113;size++)if(size!=56){
  assert(a04_iogpu56_decode(scalar,4,wire+1,size,out,64,&meta));
  assert(!memcmp(saved,out,sizeof(out)) && !memcmp(&meta,&oldmeta,sizeof(meta)));
 }
 assert(a04_iogpu56_decode(scalar,3,wire+1,56,out,64,&meta));
 assert(a04_iogpu56_decode((uint64_t *)(wire+1),4,wire+1,56,out,64,&meta));
 assert(a04_iogpu56_decode(scalar,4,(void *)(UINTPTR_MAX-8),56,out,64,&meta));
 assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,0,&meta));
 scalar[1]=1ULL<<32;assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,64,&meta));scalar[1]=0;
 for(uint64_t bad: {0ULL,65ULL,0x100000000ULL,UINT64_MAX}){
  scalar[2]=bad;assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,64,&meta));
 }
 scalar[2]=1;scalar[3]=40;assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,64,&meta));scalar[3]=56;
 assert(a04_iogpu56_decode(scalar,4,wire,56,(a04_iogpu56_record *)wire,1,&meta));
 assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,64,(a04_iogpu56_envelope *)out));
 assert(a04_iogpu56_decode(scalar,4,wire+1,56,out,64,(a04_iogpu56_envelope *)scalar));
 assert(!memcmp(saved,out,sizeof(out)) && !memcmp(&meta,&oldmeta,sizeof(meta)));
}
'''.replace('for(uint64_t bad: {0ULL,65ULL,0x100000000ULL,UINT64_MAX})','const uint64_t bads[]={0,65,0x100000000ULL,UINT64_MAX};for(uint64_t bad: bads)')
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'test.cpp').write_text(source)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(GPU),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            (p/'caller.c').write_text('''#include "A04BR3RogueIOGPUEnvelope.h"
int parse(const uint64_t *s,const void *p,size_t bytes,struct a04_iogpu56_record *o,struct a04_iogpu56_envelope *m){return a04_iogpu56_decode(s,4,p,bytes,o,64,m);}
''')
            subprocess.run(['clang','--target=arm64-apple-ios16.0','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror',
                            '-I',str(GPU),'-c',str(p/'caller.c'),'-o',str(p/'caller.o')],check=True)

if __name__=='__main__':unittest.main()
