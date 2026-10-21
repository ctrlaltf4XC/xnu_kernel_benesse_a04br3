# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess,tempfile,unittest
class WLANBootFirmware(unittest.TestCase):
    def test_checked_view_before_any_translation(self):
        inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/sdio'
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'pexpert/arm64').mkdir(parents=True)
            (d/'pexpert/pexpert.h').write_text('#include <stdint.h>\nusing vm_offset_t=uintptr_t;\nstruct {void *bootArgs;} PE_state;\n')
            (d/'pexpert/device_tree.h').write_text('using DTEntry=const void *;enum{kSuccess=1};\nint SecureDTLookupEntry(DTEntry,const char *,DTEntry *);\nint SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *);\n')
            (d/'pexpert/arm64/boot.h').write_text('struct boot_args {uint64_t physBase,memSize,topOfKernelData;};\n')
            (d/'case.cpp').write_text(r'''
#include <cassert>
#include <cstring>
#include "A04BR3WLANBootFirmware.h"
static uint64_t pairs[4][2];static boot_args ba;static unsigned char image[0x8003];
static int missing,badsize,fault;static unsigned calls;static bool ramdisk;
int SecureDTLookupEntry(DTEntry,const char *,DTEntry *p){*p=(void *)1;return kSuccess;}
int SecureDTGetProperty(DTEntry,const char *name,const void **p,unsigned *n){
 if(!strcmp(name,"RAMDisk"))return ramdisk?kSuccess:-1;
 const char *keys[]={"WLANFirmware-A04BR3","WMTFirmware-A04BR3","GPUFirmware-A04BR3","Kernel-A04BR3"};
 for(int i=0;i<4;i++)if(!strcmp(name,keys[i])){*p=pairs[i];*n=badsize==i?15:16;return missing==i?-1:kSuccess;}
 assert(false);return -1;
}
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t p){
 calls++;uint64_t off=p-pairs[0][0];assert(off<sizeof(image));
 if(fault==1)return 0;if(fault==2 && off==0x4000)return 0;
 if(fault==3 && off==sizeof(image)-1)return (uintptr_t)image+off+1;
 if(fault==4)return UINTPTR_MAX-8;return (uintptr_t)image+off;
}
static void reset(){pairs[0][0]=0x61008000;pairs[0][1]=sizeof(image);
 pairs[1][0]=0x61014000;pairs[1][1]=33;pairs[2][0]=0x61018000;pairs[2][1]=8193;
 pairs[3][0]=0x60000000;pairs[3][1]=0x101c000;ba={0x60000000,0x18000000,0x61024000};
 PE_state.bootArgs=&ba;missing=badsize=-1;fault=0;calls=0;ramdisk=false;
}
int main(){const void *view;unsigned bytes;reset();
 assert(a04_wlan_boot_firmware_view(&view,&bytes)==0 && view==image && bytes==sizeof(image) && calls==4);
 for(int i=0;i<4;i++)for(int j=0;j<2;j++){reset();if(j)badsize=i;else missing=i;
  assert(a04_wlan_boot_firmware_view(&view,&bytes)!=0 && !view && !bytes && !calls);}
 for(unsigned k=0;k<10;k++){reset();switch(k){
 case 0:pairs[0][0]+=8;break;case 1:pairs[0][1]=31;break;case 2:pairs[0][1]=16*1024*1024+1;break;
 case 3:pairs[0][0]=pairs[3][0];break;case 4:pairs[1][0]+=0x4000;break;
 case 5:pairs[2][0]+=0x4000;break;case 6:ba.topOfKernelData-=0x4000;break;
 case 7:ba.memSize=UINT64_MAX;break;case 8:ramdisk=true;break;case 9:PE_state.bootArgs=nullptr;break;}
 assert(a04_wlan_boot_firmware_view(&view,&bytes)==A04_WLAN_BOOT_RANGE && !view && !bytes && !calls);}
 for(int k=1;k<=4;k++){reset();fault=k;assert(a04_wlan_boot_firmware_view(&view,&bytes)==A04_WLAN_BOOT_MAPPING && !view && !bytes && calls);}
 assert(a04_wlan_boot_firmware_view(nullptr,&bytes)==A04_WLAN_BOOT_RANGE);
}
''')
            exe=d/'case';subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I',str(d),'-I',str(inc),str(d/'case.cpp'),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()
