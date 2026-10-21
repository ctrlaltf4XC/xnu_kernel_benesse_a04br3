# SPDX-License-Identifier: MIT
"""Pure firmware reader/selector tests; real blobs read only from external fixture tree."""
from pathlib import Path
import json
import re
import unittest
import test_wmt_protocol as codecs

FIXTURES = Path(__file__).resolve().parents[5] / 'CT3_GPL_LGPL_Src/combo_tool'

HELPERS = r'''
#include "A04BR3WMTFirmware.h"
#include <stdlib.h>
static const struct a04_wmt_identity measured={0x6630,0x8a12,0x8a11};
static inline void file(uint8_t *b,unsigned n,unsigned seq){
 memset(b,0,n);memcpy(b,"20190702b001007\n1649",20);
 b[20]=0x8a;b[21]=0x11;b[22]=0x8a;b[23]=0x11;b[24]=(uint8_t)(0x20|seq);b[26]=8;
 for(unsigned i=28;i<n;i++)b[i]=(uint8_t)(i+seq);
}
static inline void header(uint8_t *b,unsigned count,unsigned total){
 memset(b,0,total);memcpy(b,"A04WMT1",7);
 a04_wmt_put32(b+8,1);a04_wmt_put32(b+12,count);a04_wmt_put32(b+16,total);
 a04_wmt_put32(b+20,64);a04_wmt_put32(b+24,32);
}
static inline void entry(uint8_t *b,unsigned index,const char *name,unsigned off,const uint8_t *f,unsigned n){
 uint8_t *d=b+32+64*index;strcpy((char *)d,name);
 a04_wmt_put32(d+48,off);a04_wmt_put32(d+52,n);a04_wmt_put32(d+56,a04_wmt_firmware_crc32(f,n));
 memcpy(b+off,f,n);
}
static inline void bundle(uint8_t *b){
 uint8_t f[31];header(b,2,222);file(f,31,1);entry(b,0,"mt6630_patch_e3_0_hdr.bin",160,f,31);
 file(f,31,2);entry(b,1,"mt6630_patch_e3_1_hdr.bin",191,f,31);
}
static inline void refresh(uint8_t *b,unsigned index){
 uint8_t *d=b+32+64*index;unsigned off=a04_wmt_get32(d+48),n=a04_wmt_get32(d+52);
 a04_wmt_put32(d+56,a04_wmt_firmware_crc32(b+off,n));
}
'''


class WMTFirmware(unittest.TestCase):
    run_c = codecs.WMTProtocol.run_c

    def test_decoder_rom_filter_and_owned_views(self):
        self.run_c(HELPERS + r'''
int main(void){
 struct a04_wmt_revision r;assert(!a04_wmt_revision_decode(&measured,&r));
 assert(!r.exact && r.table_hardware==0x8b11 && r.eco==4 && r.patch_family==2 && r.major==0x10 && r.minor==2);
 const unsigned hw[]={0x8a00,0x8a10,0x8a11,0x8b11};
 for(unsigned i=0;i<4;i++){struct a04_wmt_identity id={0x6630,hw[i],0x8a11};assert(!a04_wmt_revision_decode(&id,&r));assert(r.exact && r.eco==i+1);}
 struct a04_wmt_identity id={0x6630,0x8a20,0x8a11};assert(a04_wmt_revision_decode(&id,&r)==A04_WMT_FW_REVISION);
 id=measured;id.chip=0x8167;assert(a04_wmt_revision_decode(&id,&r)==A04_WMT_FW_REVISION);
 id=measured;id.hardware|=0x10000;assert(a04_wmt_revision_decode(&id,&r)==A04_WMT_FW_REVISION);
 uint8_t b[222];bundle(b);struct a04_wmt_firmware_set set;
 assert(!a04_wmt_firmware_bundle(&measured,b,sizeof(b),&set) && set.count==2);
 assert(set.files[0].sequence==1 && set.files[1].sequence==2 && set.files[0].destination==0x80000);
 assert(set.files[0].header.body.data==b+188 && set.files[0].header.body.length==3);
 uint8_t copied[5]={0,0,0,0xcc,0xcc};
 assert(!a04_wmt_firmware_read(&set,1,0,copied,3));assert(!memcmp(copied,b+188,3) && copied[3]==0xcc);
 assert(a04_wmt_firmware_read(&set,1,1,copied,3)==A04_WMT_FW_RANGE);
 assert(a04_wmt_firmware_read(&set,0,0,copied,1)==A04_WMT_FW_ARGUMENT);
 assert(a04_wmt_firmware_read(&set,3,0,copied,1)==A04_WMT_FW_ARGUMENT);
 struct a04_wmt_patch_plan plan;
 assert(!a04_wmt_firmware_plan(&set,2,&plan));assert(plan.address==0x80000 && plan.length==3 && !plan.offset && !plan.pending);
 /* Caller descriptor order does not select fragment order. */
 uint8_t d[64];memcpy(d,b+32,64);memcpy(b+32,b+96,64);memcpy(b+96,d,64);
 assert(!a04_wmt_firmware_bundle(&measured,b,sizeof(b),&set));assert(set.files[0].sequence==1);
 assert(a04_wmt_firmware_crc32((const uint8_t *)"123456789",9)==0xcbf43926);
 /* The real launcher compares low8 SW/FVR, not full16 HW or SW. */
 bundle(b);b[160+22]=b[191+22]=0;refresh(b,0);refresh(b,1);
 assert(!a04_wmt_firmware_bundle(&measured,b,sizeof(b),&set));
 return 0;
}
''')

    def test_malformed_mixed_or_incompatible_bundle(self):
        self.run_c(HELPERS + r'''
int main(void){
 uint8_t b[222];struct a04_wmt_firmware_set set;
 for(unsigned fault=0;fault<24;fault++){
  bundle(b);
  switch(fault){
   case 0:b[0]='X';break;
   case 1:b[8]=2;break;
   case 2:b[12]=0;break;
   case 3:b[12]=16;break;
   case 4:b[16]--;break;
   case 5:b[20]=63;break;
   case 6:b[24]=31;break;
   case 7:b[28]=1;break;
   case 8:b[32+60]=1;break;
   case 9:a04_wmt_put32(b+32+48,159);break;
   case 10:a04_wmt_put32(b+32+48,0xffffffffu);break;
   case 11:a04_wmt_put32(b+32+52,0xffffffffu);break;
   case 12:a04_wmt_put32(b+96+48,160);break;
   case 13:b[221]^=1;break;
   case 14:b[32]='X';break;
   case 15:b[32+12]='/';break;
   case 16:memset(b+32,'A',48);break;
   case 17:b[160+23]=0x10;refresh(b,0);break; /* old E2 against real ROM11 */
   case 18:b[160+24]=0;refresh(b,0);break; /* legacy single blob */
   case 19:b[191+24]=0x21;refresh(b,1);break; /* duplicate sequence */
   case 20:b[191+24]=0x31;refresh(b,1);break; /* missing third part */
   case 21:b[191+0]^=1;refresh(b,1);break; /* mixed release */
   case 22:b[191+16]^=1;refresh(b,1);break; /* mixed platform */
   case 23:b[191+20]^=1;refresh(b,1);break; /* mixed HW */
  }
  assert(a04_wmt_firmware_bundle(&measured,b,sizeof(b),&set)!=0 && set.count==0);
 }
 bundle(b);for(unsigned n=0;n<sizeof(b);n++)assert(a04_wmt_firmware_bundle(&measured,b,n,&set)!=0);
 for(unsigned len=0;len<256;len++){
  uint8_t *q=(uint8_t *)malloc(len?len:1);assert(q);memset(q,0,len);
  (void)a04_wmt_firmware_bundle(&measured,q,len,&set);free(q);
 }
 uint32_t seed=0x8a12;
 for(unsigned iteration=0;iteration<8000;iteration++){
  bundle(b);seed=seed*1664525u+1013904223u;b[seed%sizeof(b)]^=(uint8_t)(1u<<(seed%8));
  int e=a04_wmt_firmware_bundle(&measured,b,sizeof(b),&set);
  if(!e)assert(set.count==2);else assert(!set.count);
 }
 return 0;
}
''')

    @unittest.skipUnless((FIXTURES / 'src/stp_uart_launcher.c').exists(), 'external reference source absent')
    def test_actual_launcher_comparator_and_external_blobs(self):
        launcher = (FIXTURES / 'src/stp_uart_launcher.c').read_text()
        self.assertRegex(launcher, r'#define\s+CUST_MULTI_PATCH\s+\(1\)')
        self.assertIn('lseek(patch_fd, 22, SEEK_SET)', launcher)
        self.assertIn('((patch_ver ^ fw_version) & 0x00ff)', launcher)
        self.assertIn('pi.address[0] = 0x00', launcher)
        self.assertIn('(patch_info[0] & 0xF0) >> 4', launcher)
        paths = [FIXTURES / 'patch_folder' / name for name in (
            'mt6630_patch_e3_0_hdr.bin', 'mt6630_patch_e3_1_hdr.bin',
            'mt6630_patch_e2_hdr.bin', 'mt6630_patch_e3_hdr.bin')]
        self.assertTrue(all(path.is_file() for path in paths))
        # Actual bytes are never written into source files or committed fixtures.
        declarations = 'static const char *paths[]={' + ','.join(json.dumps(str(p)) for p in paths) + '};\n'
        self.run_c(HELPERS + '#include <stdio.h>\n' + declarations + r'''
int main(void){
 uint8_t *files[4];unsigned lengths[4];
 for(unsigned i=0;i<4;i++){
  FILE *f=fopen(paths[i],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>28 && n<1000000);
  lengths[i]=(unsigned)n;files[i]=(uint8_t *)malloc(lengths[i]);assert(files[i]);rewind(f);assert(fread(files[i],1,lengths[i],f)==lengths[i]);fclose(f);
 }
 const uint8_t name[]="mt6630_patch_external.bin";struct a04_wmt_firmware_file info;
 assert(!a04_wmt_firmware_inspect(&measured,name,sizeof(name),files[0],lengths[0],&info));
 assert(info.count==2 && info.sequence==1 && info.destination==0x80000 && info.header.software==0x8a11);
 assert(!a04_wmt_firmware_inspect(&measured,name,sizeof(name),files[1],lengths[1],&info));
 assert(info.sequence==2);
 assert(a04_wmt_firmware_inspect(&measured,name,sizeof(name),files[2],lengths[2],&info)==A04_WMT_FW_ROM);
 assert(a04_wmt_firmware_inspect(&measured,name,sizeof(name),files[3],lengths[3],&info)==A04_WMT_FW_METADATA);
 unsigned total=160+lengths[0]+lengths[1];uint8_t *b=(uint8_t *)malloc(total);assert(b);header(b,2,total);
 entry(b,0,"mt6630_patch_e3_1_hdr.bin",160,files[1],lengths[1]);
 entry(b,1,"mt6630_patch_e3_0_hdr.bin",160+lengths[1],files[0],lengths[0]);
 struct a04_wmt_firmware_set set;assert(!a04_wmt_firmware_bundle(&measured,b,total,&set));
 assert(set.count==2 && set.files[0].header.body.length==49720 && set.files[1].header.body.length==65520);
 uint8_t part[1000];assert(!a04_wmt_firmware_read(&set,2,65000,part,520));
 assert(!memcmp(part,files[1]+28+65000,520));
 free(b);for(unsigned i=0;i<4;i++)free(files[i]);return 0;
}
''')
