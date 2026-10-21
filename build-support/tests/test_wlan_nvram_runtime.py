# SPDX-License-Identifier: MIT
"""Execute the real read-only runtime parser with an instrumented partition.
No extracted production calibration is generated or required.
"""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
BLOCK=0x20000
ROOT=Path(__file__).resolve().parents[2]

def image():
    p=bytearray(BLOCK*8);record=bytearray(514)
    struct.pack_into('<HH',record,0,0x106,0);record[4:10]=bytes.fromhex('021122334455');record[196]=1
    struct.pack_into('<HH',record,256,1,0);check=0
    for i,v in enumerate(record[:512]):check=((check+v) if i%2==0 else check^v)&255
    record[512:]=bytes([0xaa,check]);p[BLOCK:BLOCK+514]=record
    struct.pack_into('<III',p,0,192,len(record),0xffffffff)
    checksum=0
    for i in range(0,512,4):
        w=struct.unpack_from('<I',record,i)[0];checksum=checksum^w if i%8==0 else (checksum+w)&0xffffffff
    checksum=(checksum+int.from_bytes(record[512:],'little'))&0xffffffff
    struct.pack_into('<I',p,12,checksum^len(record));struct.pack_into('<III',p,64,7,0,514)
    name=b'/data/nvram/APCFG/APRDEB/WIFI\0';p[76:76+len(name)]=name
    p[BLOCK*4:BLOCK*4+15]=bytes(15);struct.pack_into('<I',p,BLOCK*4+15,0xaabbccdd)
    return p

class RuntimeNVRAM(unittest.TestCase):
    def test_actual_gpt_discovery_never_enables_nvram_writes(self):
        source=(ROOT/'bsd/dev/a04br3_msdc.c').read_text()
        function=source[source.index('static int find_root_partition('):source.index('int a04br3_emmc_rootdev(dev_t *dev);')]
        c=r'''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
static uint32_t root_first,root_sectors,nvram_first,nvram_sectors;
static uint8_t disk[512];
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t le64(const uint8_t *p){return le32(p)|((uint64_t)le32(p+4)<<32);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){put32(p,n);put32(p+4,n>>32);}
static int read_block(unsigned op,uint32_t lba,uint8_t *out){assert(op==17 && lba==2);memcpy(out,disk,512);return 0;}
static void entry(unsigned index,const char *name,unsigned first,unsigned last){uint8_t *p=disk+index*128;memset(p,0,128);put64(p+32,first);put64(p+40,last);for(unsigned i=0;name[i];i++)p[56+2*i]=name[i];}
static int probe(void){uint8_t h[512]={0};uint32_t crc=~0u;for(unsigned i=0;i<512;i++){crc^=disk[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}put64(h+72,2);put32(h+80,4);put32(h+84,128);put32(h+88,~crc);put64(h+40,34);put64(h+48,9990);root_first=root_sectors=nvram_first=nvram_sectors=0;return find_root_partition(h,10000);}
''' 
        # Forward declaration keeps the real copied function after test helpers.
        c=c.replace('static int probe(void)', 'static int find_root_partition(const uint8_t [512],uint32_t);\nstatic int probe(void)')
        c+=function+r'''
int main(void){
 entry(0,"userdata",4000,9000);entry(1,"nvram",1000,1999);assert(probe()==0 && nvram_first==1000 && nvram_sectors==1000);
 entry(2,"nvram",2000,2999);assert(probe()==0 && !nvram_sectors);
 entry(2,"unused",2000,2999);entry(1,"nvram",3900,4100);assert(probe()==0 && !nvram_sectors);
 entry(1,"nvram",20,30);assert(probe()==0 && !nvram_sectors);
 entry(1,"other",1000,1999);assert(probe()==0 && !nvram_sectors);
 return 0;}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'test.c').write_text(c)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True)
            subprocess.run([str(d/'test')],check=True,stdout=subprocess.DEVNULL)

    def test_actual_parser_reads_checksums_and_failures(self):
        c=r'''
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "a04br3_wlan_nvram.h"
struct input {unsigned char *p;unsigned size,calls,fail;};
static int readpart(void *arg,uint32_t off,void *out,unsigned n){
 struct input *i=arg;assert(off<=i->size && n<=i->size-off);
 i->calls++;if(i->calls==i->fail)return EIO;
 memcpy(out,i->p+off,n);return 0;
}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 struct input i={calloc(1,n),(unsigned)n,0,0};assert(i.p);assert(fread(i.p,1,n,f)==(size_t)n);fclose(f);
 struct a04_nvram_work *w=calloc(1,sizeof(*w));assert(w);uint8_t out[512]={0};
 struct a04_nvram_reader r={&i,i.size,readpart};assert(a04_nvram_wifi(&r,w,out)==0);assert(out[196]==1);
 unsigned calls=i.calls;
 for(unsigned at=1;at<=calls;at++){
  memset(out,0,sizeof(out));memset(w,0,sizeof(*w));i.calls=0;i.fail=at;
  assert(a04_nvram_wifi(&r,w,out)==EIO);for(unsigned j=0;j<512;j++)assert(out[j]==0);
 }
 i.fail=0;
 /* Corrupt map size, bounds, pathname, data checksum, calibration/trailer,
  * and the three-copy bitmap. Every case must reject without publishing. */
 unsigned faults[]={0,4,8,12,68,72,76,0x20000,0x20000+513,0x80000+5,0x80000+15};
 for(unsigned k=0;k<sizeof(faults)/sizeof(*faults);k++){
  unsigned at=faults[k];i.p[at]^=0x40;memset(out,0,512);i.calls=0;
  assert(a04_nvram_wifi(&r,w,out)!=0);for(unsigned j=0;j<512;j++)assert(out[j]==0);i.p[at]^=0x40;
 }
 /* Same logical file relocated past a bitmap-marked bad block. */
 memcpy(i.p+0x40000,i.p+0x20000,514);memset(i.p+0x20000,0xff,514);
 for(unsigned k=0;k<3;k++)i.p[0x80000+k*5+1]=1;
 memset(out,0,512);assert(a04_nvram_wifi(&r,w,out)==0 && out[196]==1);
 r.bytes--;assert(a04_nvram_wifi(&r,w,out)==EINVAL);
 free(w);free(i.p);return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'input').write_bytes(image());(d/'test.c').write_text(c)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(ROOT/'bsd/dev'),str(d/'test.c'),'-o',str(d/'test')],check=True)
            subprocess.run([str(d/'test'),str(d/'input')],check=True)

if __name__=='__main__':unittest.main()
