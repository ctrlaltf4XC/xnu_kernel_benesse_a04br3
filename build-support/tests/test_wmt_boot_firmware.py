# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual WMT DT view helper: no PA translation before complete range proof."""
from pathlib import Path
import subprocess,tempfile,unittest

class WMTBootFirmware(unittest.TestCase):
    def test_dt_mapping_and_range(self):
        inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/sdio'
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'pexpert/arm64').mkdir(parents=True)
            (root/'pexpert/pexpert.h').write_text('#include <stdint.h>\nusing vm_offset_t=uintptr_t;\nstruct {void *bootArgs;} PE_state;\n')
            (root/'pexpert/device_tree.h').write_text('using DTEntry=const void *;enum{kSuccess=1};\nint SecureDTLookupEntry(DTEntry,const char *,DTEntry *);\nint SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *);\n')
            (root/'pexpert/arm64/boot.h').write_text('struct boot_args {uint64_t physBase,memSize,topOfKernelData;};\n')
            (root/'test.cpp').write_text(r'''
#include <cstring>
#include <cassert>
#include "A04BR3WMTBootFirmware.h"
static uint64_t values[3][2];static boot_args ba;
static bool absentMap,ramdisk;static int absentProperty,badSize,nullProperty,mapFault;
static unsigned translations;static unsigned char image[0x8003];
int SecureDTLookupEntry(DTEntry,const char *s,DTEntry *p){assert(!strcmp(s,"/chosen/memory-map"));*p=(DTEntry)1;return absentMap?-1:kSuccess;}
int SecureDTGetProperty(DTEntry,const char *s,const void **p,unsigned *n){
 if(!strcmp(s,"RAMDisk"))return ramdisk?kSuccess:-1;
 int i=!strcmp(s,"WMTFirmware-A04BR3")?0:!strcmp(s,"GPUFirmware-A04BR3")?1:2;
 if(i==2)assert(!strcmp(s,"Kernel-A04BR3"));
 *p=i==nullProperty?nullptr:values[i];*n=i==badSize?15:16;return i==absentProperty?-1:kSuccess;
}
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t pa){
 translations++;uint64_t off=pa-values[0][0];assert(off<values[0][1]);
 if(mapFault==1)return 0;
 if(mapFault==2 && off==0x4000)return 0; // interior hole despite valid endpoints
 if(mapFault==3 && off==values[0][1]-1)return (vm_offset_t)image+off+1;
 if(mapFault==4)return UINTPTR_MAX-8;
 return (vm_offset_t)image+off;
}
static void reset(){
 values[0][0]=0x61008000;values[0][1]=sizeof(image);
 values[1][0]=0x61014000;values[1][1]=8193;
 values[2][0]=0x60000000;values[2][1]=0x1018000;
 ba={0x60000000,0x18000000,0x61020000};PE_state.bootArgs=&ba;
 absentMap=ramdisk=false;absentProperty=badSize=nullProperty=-1;mapFault=0;translations=0;
}
int main(){
 const void *p;unsigned n;reset();assert(a04_wmt_boot_firmware_view(&p,&n)==0 && p==image && n==sizeof(image) && translations==4);
 reset();absentMap=true;p=(void *)1;n=99;assert(a04_wmt_boot_firmware_view(&p,&n)==A04_WMT_BOOT_ABSENT && !p && !n && !translations);
 for(int i=0;i<3;i++)for(int kind=0;kind<3;kind++){
  reset();if(kind==0)absentProperty=i;else if(kind==1)badSize=i;else nullProperty=i;
  assert(a04_wmt_boot_firmware_view(&p,&n)!=0 && !p && !n && !translations);
 }
 for(unsigned fault=0;fault<19;fault++){
  reset();switch(fault){
   case 0:values[0][0]+=8;break;case 1:values[0][1]=31;break;
   case 2:values[0][1]=32*1024*1024+1;break;case 3:values[1][0]+=0x4000;break;
   case 4:values[1][1]=4096;break;case 5:values[1][1]=32*1024*1024+1;break;
   case 6:values[2][0]+=0x4000;break;case 7:values[2][1]--;break;
   case 8:values[2][1]=UINT64_MAX;break;case 9:ba.memSize=UINT64_MAX;break;
   case 10:ba.physBase=UINT64_MAX;break;case 11:ba.topOfKernelData-=0x4000;break;
   case 12:ba.topOfKernelData+=0x4000;break;case 13:ba.memSize=0x1000000;break;
   case 14:PE_state.bootArgs=nullptr;break;case 15:ramdisk=true;break;
   case 16:values[0][0]=values[2][0];break;
   case 17:values[1][0]=0xffffc000;break;case 18:ba.memSize=0x100000000ULL;break;
  }
  assert(a04_wmt_boot_firmware_view(&p,&n)==A04_WMT_BOOT_RANGE && !p && !n && !translations);
 }
 for(int fault=1;fault<=4;fault++){reset();mapFault=fault;assert(a04_wmt_boot_firmware_view(&p,&n)==A04_WMT_BOOT_MAPPING && !p && !n && translations);}
 assert(a04_wmt_boot_firmware_view(nullptr,&n)==A04_WMT_BOOT_RANGE);
}
''')
            exe=root/'case'
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),'-I',str(inc),str(root/'test.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
            # The pure range half remains usable by the C shim contract tests.
            (root/'range.c').write_text('#include "A04BR3WMTBootFirmware.h"\nint main(void){return !a04_wmt_boot_firmware_range(0x61008000,0x8003,0x61014000,8193,0x60000000,0x1018000,0x60000000,0x18000000,0x61020000);}\n')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(inc),str(root/'range.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
