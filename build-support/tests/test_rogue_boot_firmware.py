# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Compile the actual GPU CPP delivery path against DT/translation/memory shims.

This verifies validation-before-translation and stage dispatch/status, not real
page tables or GPU execution. Memory stage contents have separate host tests.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class RogueBootFirmware(unittest.TestCase):
    def test_driver_dt_reservation_translation_and_stage_dispatch(self):
        inc=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for d in ('IOKit','pexpert/arm64'): (root/d).mkdir(parents=True,exist_ok=True)
            (root/'IOKit/IOLib.h').write_text(r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
using vm_offset_t=uintptr_t;using vm_size_t=uintptr_t;
static void IODelay(unsigned) {}
static void IOLog(const char *,...) {}
''')
            (root/'IOKit/IOService.h').write_text(r'''
#define OSDeclareDefaultStructors(c)
#define OSDefineMetaClassAndStructors(c,b)
#define APPLE_KEXT_OVERRIDE override
static const void *kOSBooleanFalse=(void *)1;
static char payloadStatus[64],bringupStage[64];
static uint64_t stageResult;static unsigned runningFalse;
class IOService {
public:
 virtual bool start(IOService *) { return true; }
 virtual void stop(IOService *) {}
 void registerService() {}
 void setProperty(const char *name,const char *value) {
  if(!strcmp(name,"FirmwarePayloadStatus"))strcpy(payloadStatus,value);
  if(!strcmp(name,"BringupStage"))strcpy(bringupStage,value);
 }
 void setProperty(const char *name,const void *value) {
  if(!strcmp(name,"FirmwareRunning")){assert(value==kOSBooleanFalse);++runningFalse;}
 }
 void setProperty(const char *name,uint64_t value,int) {
  if(!strcmp(name,"FirmwareStageResult"))stageResult=value;
 }
};
''')
            (root/'pexpert/pexpert.h').write_text(r'''
static bool PE_parse_boot_argn(const char *,void *,unsigned) { return false; }
struct { void *bootArgs; } PE_state;
''')
            (root/'pexpert/device_tree.h').write_text(r'''
using DTEntry=const void *;
enum { kSuccess=1 };
int SecureDTLookupEntry(DTEntry,const char *,DTEntry *);
int SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *);
''')
            (root/'pexpert/arm64/boot.h').write_text(
                'struct boot_args { uint64_t physBase,memSize,topOfKernelData; };\n')
            (root/'A04BR3RogueMemory.h').write_text(r'''
#include "A04BR3RogueFirmware.h"
static int stages,resets,stageError;
static const void *stageInput;static size_t stageLength;
class A04BR3RogueMemory {
public:
 enum { TableBytes=32768,CodeBytes=102400,DataBytes=20480 };
 bool reserve(){return true;}
 void reset(){++resets;}
 int stage(const void *p,size_t n){++stages;stageInput=p;stageLength=n;
  if(stageError){reset();}return stageError;}
};
''')
            (root/'A04BR3RoguePower.h').write_text(r'''
struct a04_rgx_io { void *p;uint32_t (*read)(void *,unsigned,unsigned);
 void (*write)(void *,unsigned,unsigned,uint32_t);void (*delay)(void *,unsigned); };
static int a04_rgx_power_on(a04_rgx_io *) {return 0;}
''')
            source=(inc/'A04BR3RogueGPU.cpp').read_text()
            source=source.replace('private:', 'public:')
            source=re.sub(r'__asm__ volatile\([^;]*\);','',source)
            (root/'driver.cpp').write_text(source)
            (root/'test.cpp').write_text(r'''
#include "driver.cpp"
static uint64_t asset[2],reservation[2];
static unsigned assetSize,reservationSize;
static bool absentMap,absentAsset,absentKernel,badTranslation,zeroTranslation,hasRAMDisk;
static unsigned translations;
static boot_args ba;
static unsigned char image[8192];
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t){return 0;}
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t pa) {
 ++translations;
 assert(pa==asset[0] || pa==asset[0]+asset[1]-1);
 if(zeroTranslation)return 0;
 return (vm_offset_t)image+(pa-asset[0])+((badTranslation && pa!=asset[0])?1:0);
}
int SecureDTLookupEntry(DTEntry,const char *path,DTEntry *out) {
 assert(!strcmp(path,"/chosen/memory-map"));*out=(DTEntry)1;return absentMap?-1:kSuccess;
}
int SecureDTGetProperty(DTEntry entry,const char *name,const void **out,unsigned *size) {
 assert(entry==(DTEntry)1);
 if(!strcmp(name,"GPUFirmware-A04BR3")){
  *out=asset;*size=assetSize;return absentAsset?-1:kSuccess;
 }
 if(!strcmp(name,"RAMDisk")){*out=asset;*size=16;return hasRAMDisk?kSuccess:-1;}
 assert(!strcmp(name,"Kernel-A04BR3"));*out=reservation;*size=reservationSize;
 return absentKernel?-1:kSuccess;
}
static void initialize() {
 asset[0]=0x61008000;asset[1]=sizeof(image);
 reservation[0]=0x60000000;reservation[1]=0x100c000;
 assetSize=reservationSize=16;
 ba={0x60000000,0x18000000,0x61014000};PE_state.bootArgs=&ba;
 absentMap=absentAsset=absentKernel=badTranslation=zeroTranslation=hasRAMDisk=false;
 stages=resets=stageError=translations=runningFalse=0;stageResult=99;
 memset(payloadStatus,0,sizeof(payloadStatus));memset(bringupStage,0,sizeof(bringupStage));
}
int main() {
 A04BR3RogueGPU gpu;
 initialize();gpu.stageBootFirmware();
 assert(stages==1 && translations==2 && !resets && !stageResult && runningFalse==1);
 assert(stageInput==image && stageLength==sizeof(image));
 assert(!strcmp(payloadStatus,"cpu-staged") && !strcmp(bringupStage,"firmware-cpu-staged"));
 for(unsigned mode=0;mode<2;mode++){
  initialize();if(mode)absentAsset=true;else absentMap=true;
  gpu.stageBootFirmware();assert(!stages && !translations && !resets);
  assert(!strcmp(payloadStatus,"absent"));
 }
 for(unsigned fault=0;fault<17;fault++) {
  initialize();
  switch(fault){
   case 0:assetSize=15;break;case 1:reservationSize=15;break;
   case 2:absentKernel=true;break;case 3:PE_state.bootArgs=nullptr;break;
   case 4:asset[0]+=8;break;case 5:asset[1]=4096;break;
   case 6:asset[1]=32*1024*1024+1;break;
   case 7:asset[0]=0x61004000;break;case 8:reservation[0]+=0x4000;break;
   case 9:ba.topOfKernelData-=0x4000;break;case 10:ba.memSize=0x1000000;break;
   case 11:reservation[1]=UINT64_MAX;break;case 12:ba.physBase=UINT64_MAX;break;
   case 13:asset[0]=UINT64_MAX-0x3fff;break;
   case 14:reservation[1]=0x100c001;break;case 15:asset[1]=0;break;case 16:hasRAMDisk=true;break;
  }
  gpu.stageBootFirmware();assert(!stages && !translations && resets==1);
  assert(!strcmp(payloadStatus,"rejected-reservation"));
 }
 for(unsigned fault=0;fault<2;fault++){
  initialize();if(fault)badTranslation=true;else zeroTranslation=true;
  gpu.stageBootFirmware();assert(!stages && resets==1 && translations);
 }
 initialize();stageError=3;gpu.stageBootFirmware();
 assert(stages==1 && resets==1 && stageResult==3 && runningFalse==1);
 assert(!strcmp(payloadStatus,"rejected-image") && !strcmp(bringupStage,"firmware-stage-failed"));
}
''')
            exe=root/'test'
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-I',str(root),'-I',str(inc),
                            str(root/'test.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
