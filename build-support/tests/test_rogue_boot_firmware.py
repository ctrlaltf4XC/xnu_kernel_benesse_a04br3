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
            (root/'IOKit/IOLocks.h').write_text('struct IOLock {};\nstatic bool lockHeld,forceLockBusy;\nstatic IOLock *IOLockAlloc(){return new IOLock;}\nstatic void IOLockFree(IOLock *p){delete p;}\nstatic void IOLockLock(IOLock *){assert(!lockHeld);lockHeld=true;}\nstatic bool IOLockTryLock(IOLock *){if(lockHeld || forceLockBusy)return false;lockHeld=true;return true;}\nstatic void IOLockUnlock(IOLock *){assert(lockHeld);lockHeld=false;}\n')
            (root/'arm').mkdir(exist_ok=True)
            (root/'arm/machine_routines.h').write_text('static bool inInterrupt;\nstatic bool ml_at_interrupt_context(){return inInterrupt;}\n')
            (root/'IOKit/IOService.h').write_text(r'''
#define OSDeclareDefaultStructors(c)
#define OSDefineMetaClassAndStructors(c,b)
#define APPLE_KEXT_OVERRIDE override
static const void *kOSBooleanFalse=(void *)1;
static const void *kOSBooleanTrue=(void *)2;
static char payloadStatus[64],bringupStage[64];
static uint64_t memoryBytes,renderMask,priorityMask;
static unsigned clearCalls;static int clearError;
static unsigned transferCalls;static int transferError;
static unsigned priorityCalls;static int priorityError;
static uint64_t stageResult;static unsigned runningFalse,runningTrue;
using IOReturn=int;
 enum {kIOReturnSuccess=0,kIOReturnBadArgument=1,kIOReturnNotPermitted=2,kIOReturnTimeout=3,
       kIOReturnBusy=4,kIOReturnNotReady=5,kIOReturnError=6};
 class OSSymbol {const char *s;public:OSSymbol(const char *p):s(p){}
 bool isEqualTo(const char *p)const{return !strcmp(s,p);}};
 class IOService {
public:
 virtual void free(){}
 virtual IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *){return 99;}
 virtual bool start(IOService *) { return true; }
 virtual void stop(IOService *) {}
 void registerService() {}
 void setProperty(const char *name,const char *value) {
  if(!strcmp(name,"FirmwarePayloadStatus"))strcpy(payloadStatus,value);
  if(!strcmp(name,"BringupStage"))strcpy(bringupStage,value);
 }
 void setProperty(const char *name,const void *value) {
  if(!strcmp(name,"FirmwareRunning")){
   if(value==kOSBooleanFalse)++runningFalse;else {assert(value==kOSBooleanTrue);++runningTrue;}
  }
 }
 void setProperty(const char *name,uint64_t value,int) {
  if(!strcmp(name,"FirmwareStageResult"))stageResult=value;
  if(!strcmp(name,"FirmwareMemoryBytes"))memoryBytes=value;
  if(!strcmp(name,"FirmwareRenderUpdateVerifiedDMs"))renderMask=value;
  if(!strcmp(name,"FirmwarePriorityVerifiedDMs"))priorityMask=value;
 }
};
''')
            (root/'pexpert/pexpert.h').write_text(r'''
static unsigned autoStart;
static bool PE_parse_boot_argn(const char *name,void *out,unsigned) {
 if(!strcmp(name,"a04br3.gpu_start")){*(unsigned *)out=autoStart;return true;}return false;
}
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
            (root/'A04BR3RogueRenderMemory.h').write_text(r'''
#pragma once
#include "A04BR3RogueRenderSubmit.h"
#include "A04BR3RogueReadback.h"
static unsigned coldPrepares,renderPins,renderResets,renderUpdates;
static bool coldOK=true;static int renderError;
class A04BR3RogueRenderMemory {public:
 a04_rgx_render_observation terminal={};
 const a04_rgx_render_observation &observation()const{return terminal;}
 bool clearPrepared()const{return coldOK;}
 unsigned clearedWords()const{return clearError?0:1024;}
 unsigned guardErrors()const{return 0;}
 unsigned beforeA5()const{return 0;} unsigned afterA5()const{return 0;}
 uint32_t firstWord()const{return 0;}uint32_t lastWord()const{return 0;}
 int readbackResult()const{return 0;}bool readbackPixelsValid()const{return true;}
 a04_rgx_readback_observation cache={};
 const a04_rgx_readback_observation &readbackObservation()const{return cache;}
 unsigned ownedBytes()const{return coldOK?290816:0;}
 bool prepareCold(A04BR3RogueMemory &,unsigned id,uint64_t pdsVA,uint64_t uscVA){assert(id==1 && pdsVA==0xa800000000ULL && uscVA==0xba00000000ULL);++coldPrepares;return coldOK;}
 void pinForFirmware(){++renderPins;}
 void reset(){++renderResets;}
 const void *view()const{return coldOK?this:nullptr;}
};
''')
            (root/'A04BR3RogueRuntime.h').write_text(r'''
#include "A04BR3RogueRenderMemory.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueStartupExecutor.h"
#include "A04BR3RogueCommand.h"
static unsigned fenceCalls,lastDM;static int fenceError;
static unsigned runtimePrepares,runtimeStarts;static bool prepareOK=true;static int startError;
class A04BR3RogueRuntime {
public:
 void reset(){}
 bool prepare(A04BR3RogueMemory &,uint32_t){assert(coldPrepares==1);++runtimePrepares;return prepareOK;}
 int start(A04BR3RogueMemory &,const a04_rgx_exec_io &){assert(renderPins==1);++runtimeStarts;return startError;}
 int fence(uint32_t dm,const a04_rgx_command_io &){assert(lockHeld);++fenceCalls;lastDM=dm;return fenceError;}
 int renderUpdate(A04BR3RogueRenderMemory &,uint32_t dm,const a04_rgx_command_io &,uint32_t priority=0xffffffffU,bool transfer=false){
  assert(lockHeld);
  if(transfer){assert(dm==3 && priority<=1);++transferCalls;return transferError;}
  if(priority!=0xffffffffU){assert((dm==2 || dm==3) && priority<=1);++priorityCalls;return priorityError;}
  assert(dm==0);++renderUpdates;return renderError;}
 int renderClear(A04BR3RogueRenderMemory &,const a04_rgx_command_io &){assert(lockHeld);++clearCalls;return clearError;}
 uint32_t readShared(uint32_t){return 0;}
 uint32_t ready(){return 0;}
};
''')
            (root/'A04BR3RoguePower.h').write_text(r'''
enum {A04_RGX_TOP=1};
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
static uint32_t gicMemory[1024],coreMemory[0x10000/4],topMemory[1024];
extern "C" vm_offset_t ml_io_map(vm_offset_t pa,vm_size_t){return pa==0x10310000?(vm_offset_t)gicMemory:0;}
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
 stages=resets=stageError=translations=runningFalse=runningTrue=0;stageResult=99;
 fenceCalls=lastDM=0;fenceError=0;inInterrupt=false;
 assert(!lockHeld);forceLockBusy=false;memoryBytes=renderMask=priorityMask=0;priorityCalls=transferCalls=clearCalls=0;priorityError=transferError=clearError=0;
 coldPrepares=renderPins=renderResets=renderUpdates=0;coldOK=true;renderError=0;
 autoStart=runtimePrepares=runtimeStarts=0;prepareOK=true;startError=0;
 memset(gicMemory,0,sizeof(gicMemory));memset(coreMemory,0,sizeof(coreMemory));
 memset(topMemory,0,sizeof(topMemory));topMemory[0]=4U<<8;topMemory[1]=8U<<8;
 memset(payloadStatus,0,sizeof(payloadStatus));memset(bringupStage,0,sizeof(bringupStage));
}
int main() {
 A04BR3RogueGPU gpu;gpu.regs[A04_RGX_TOP]=(vm_offset_t)topMemory;
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
 initialize();autoStart=1;gpu.core=(vm_offset_t)coreMemory;gpu.stageBootFirmware();
 assert(runtimePrepares==1 && runtimeStarts==1 && runningTrue==1);
 assert(!strcmp(bringupStage,"firmware-ready"));
 assert(fenceCalls==8 && lastDM==3 && priorityCalls==4 && transferCalls==2 && clearCalls==1 && priorityMask==0xc && renderUpdates==1 && renderMask==1 && memoryBytes==790528);
 forceLockBusy=true;
 assert(gpu.submitRenderUpdate(2)==A04_CCB_BUSY && renderUpdates==1);
 assert(gpu.submitFence(3)==A04_CCB_BUSY && fenceCalls==8);forceLockBusy=false;
 assert(gpu.callPlatformFunction(nullptr,false,nullptr,nullptr,nullptr,nullptr)==kIOReturnBadArgument);
 OSSymbol fenceName("A04BR3RogueFence"),otherName("unknown");
 assert(gpu.callPlatformFunction(&otherName,false,nullptr,nullptr,nullptr,nullptr)==99);
 assert(gpu.callPlatformFunction(&fenceName,false,(void*)4,nullptr,nullptr,nullptr)==kIOReturnBadArgument);
 assert(gpu.callPlatformFunction(&fenceName,false,nullptr,(void*)1,nullptr,nullptr)==kIOReturnBadArgument);
 inInterrupt=true;
 assert(gpu.callPlatformFunction(&fenceName,false,nullptr,nullptr,nullptr,nullptr)==kIOReturnNotPermitted);
 assert(fenceCalls==8);inInterrupt=false;
 assert(gpu.callPlatformFunction(&fenceName,false,(void*)2,nullptr,nullptr,nullptr)==0 && lastDM==2);
 fenceError=A04_CCB_TIMEOUT;
 assert(gpu.callPlatformFunction(&fenceName,false,(void*)3,nullptr,nullptr,nullptr)==kIOReturnTimeout);
 fenceError=A04_CCB_CORRUPT;
 assert(gpu.callPlatformFunction(&fenceName,false,(void*)3,nullptr,nullptr,nullptr)==kIOReturnError);
 assert(gicMemory[(0x180+6*4)/4]==(1U<<25));
 assert(!gicMemory[(0x100+6*4)/4]); /* no CPU SPI enable */
 coreMemory[0x8a8/4]=1;
 assert(!a04br3_rogue_irq_dispatch(216));assert(a04br3_rogue_irq_dispatch(217));
 assert(coreMemory[0x8b0/4]==1);
 gpu.stop(nullptr);assert(renderResets==1 && !lockHeld && !a04br3_rogue_irq_dispatch(217));
 assert(gpu.callPlatformFunction(&fenceName,false,nullptr,nullptr,nullptr,nullptr)==kIOReturnNotReady);
 initialize();autoStart=1;prepareOK=false;gpu.stageBootFirmware();
 assert(runtimePrepares==1 && !runtimeStarts && !runningTrue && renderResets==1);
 initialize();autoStart=1;coldOK=false;gpu.stageBootFirmware();
 assert(runtimeStarts==1 && runningTrue==1 && fenceCalls==3 && !renderUpdates && memoryBytes==499712);
 initialize();autoStart=1;renderError=A04_CCB_TIMEOUT;gpu.stageBootFirmware();
 assert(runtimeStarts==1 && fenceCalls==3 && renderUpdates==1);
 initialize();autoStart=1;priorityError=A04_CCB_TIMEOUT;gpu.stageBootFirmware();
 assert(runtimeStarts==1 && fenceCalls==4 && priorityCalls==1 && !priorityMask);
 initialize();autoStart=1;fenceError=A04_CCB_TIMEOUT;gpu.stageBootFirmware();
 assert(runtimeStarts==1 && fenceCalls==1 && !renderUpdates);
 initialize();autoStart=1;startError=1;gpu.stageBootFirmware();
 assert(runtimeStarts==1 && renderPins==1 && !fenceCalls && !renderUpdates);
 initialize();autoStart=1;gicMemory[(0x100+6*4)/4]=1U<<25;gpu.stageBootFirmware();
 assert(!runtimePrepares && !runtimeStarts && !runningTrue);
 assert(!strcmp(bringupStage,"firmware-irq-mask-failed"));
 for(unsigned fault=0;fault<3;fault++){
  initialize();autoStart=1;
  if(fault==0)topMemory[0]=0;else if(fault==1)topMemory[1]=0;else topMemory[0x40/4]=1U<<18;
  gpu.stageBootFirmware();assert(!runtimePrepares && !runtimeStarts && !runningTrue);
  assert(!strcmp(bringupStage,"firmware-clock-readback-failed"));
 }
 gpu.free();
}
''')
            exe=root/'test'
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-I',str(root),'-I',str(inc),
                            str(root/'test.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
