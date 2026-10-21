# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess
import tempfile
import unittest

ARM=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm'

class Restart(unittest.TestCase):
    def test_order_keys_mask_delay_and_failed_readback(self):
        code=r'''
#include <assert.h>
#include "A04BR3Restart.h"
static uint32_t original,mode;static unsigned at;static bool fail;
static unsigned events[16];
static void barrier(void*){events[at++]=1;}
static uint32_t read(void*,unsigned off){assert(off==0);events[at++]=2;return at==4?original:(fail?0:mode);}
static void write(void*,unsigned off,uint32_t v){
 events[at++]=3;
 if(off==8)assert(v==0x1971 && at==2);
 else if(off==0){mode=v;assert(at==5 && (mode>>24)==0x22 && (mode&0x5d)==0x14);}
 else{assert(off==0x14 && v==0x1209 && at==10);}
}
static void delay(void*,unsigned us){assert(us==100 && at==7);events[at++]=4;}
int main(){a04_restart_io io={nullptr,read,write,barrier,delay};
 for(unsigned n=0;n<256;n++){
  original=0xa5fedc00U|n;at=0;fail=false;
  assert(a04_restart_request(&io));assert(at==11);
  assert((mode&0xffffa2U)==(original&0xffffa2U));
  const unsigned expected[]={1,3,1,2,3,1,2,4,1,3,1};
  for(unsigned i=0;i<11;i++)assert(events[i]==expected[i]);
 }
 at=0;fail=true;assert(!a04_restart_request(&io) && at==7); // no SWRST
 at=0;io.delay_us=nullptr;assert(!a04_restart_request(&io) && !at);
 assert(!a04_restart_request(nullptr));
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'test.cpp').write_text(code)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ARM),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

    def test_actual_platform_dispatch_and_arm64_compile(self):
        stub=r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
#define APPLE_KEXT_OVERRIDE override
#define OSDeclareDefaultStructors(x)
#define OSDefineMetaClassAndStructors(x,y)
using vm_offset_t=uintptr_t;using vm_size_t=uintptr_t;
extern size_t strlcpy(char *,const char *,size_t);
extern void IOLog(const char *,...);extern void IODelay(unsigned);
static int delegated;static unsigned lastType;
enum {kPEHaltCPU,kPERestartCPU,kPEHangCPU,kPEUPSDelayHaltCPU,kPEPanicRestartCPU,kPEPanicSync,kPEPagingOff,kPEPanicBegin,kPEPanicEnd,kPEPanicRestartCPUNoCallouts,kPEPanicDiagnosticsDone,kPEPanicDiagnosticsInProgress};
static const char *gIODTPlane="IODeviceTree";enum{kIODTExclusive=1};
class IORegistryEntry {public:IORegistryEntry *childFromPath(const char*,const char*){return nullptr;}void release(){}};
class IOService:public IORegistryEntry {};
static inline void *IODTFindMatchingEntries(IORegistryEntry*,int,const char*){return nullptr;}
class IODTPlatformExpert:public IOService {public:
 virtual bool start(IOService*){return true;}void registerService(){}
 void createNubs(IORegistryEntry*,void*){}
 virtual int haltRestart(unsigned type){++delegated;lastType=type;return type==kPEPanicSync?0:-77;}
 virtual void processTopLevel(IORegistryEntry*){}virtual const char *deleteList(){return "";}
 virtual const char *excludeList(){return "";}virtual bool getTargetName(char*,int){return false;}
 virtual bool getProductName(char*,int){return false;}
};
'''
        host=r'''
#include <assert.h>
#include <string.h>
#include "A04BR3PlatformExpert.cpp"
static uint32_t regs[1024];static unsigned maps,delays;static bool mapOK=true;
extern "C" vm_offset_t ml_io_map(vm_offset_t pa,vm_size_t bytes){assert(pa==0x10007000 && bytes==4096);++maps;return mapOK?(uintptr_t)regs:0;}
void IOLog(const char*,...){}void IODelay(unsigned us){delays+=us;}
size_t strlcpy(char *o,const char *s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(o,s,k);o[k]=0;}return len;}
int main(){A04BR3PlatformExpert p;IOService provider;assert(p.start(&provider) && maps==1);
 for(unsigned t=0;t<12;t++)if(t!=kPERestartCPU){
  int before=delegated;assert(p.haltRestart(t)==(t==kPEPanicSync?0:-77));
  assert(delegated==before+1 && lastType==t && !regs[0] && !regs[2] && !regs[5] && !delays);
 }
 assert(p.haltRestart(999)==-77 && lastType==999 && !regs[5]);
 assert(p.haltRestart(kPERestartCPU)==-1); // a host mock cannot actually reset
 assert(regs[0]==0x22000014 && regs[2]==0x1971 && regs[5]==0x1209 && delays==1100);
 mapOK=false;A04BR3PlatformExpert absent;assert(absent.start(&provider));
 regs[5]=0;delays=0;assert(absent.haltRestart(kPERestartCPU)==-1 && !regs[5] && !delays);
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'IOKit').mkdir();(p/'pexpert/arm64').mkdir(parents=True)
            (p/'pexpert/arm64/board_config.h').write_text('#define A04BR3 1\n')
            for name in ['IOPlatformExpert.h','IODeviceTreeSupport.h','IOLib.h']:(p/'IOKit'/name).write_text('#include "stub.h"\n')
            (p/'stub.h').write_text(stub)
            # Only host harness replaces the ARM barrier; target compile keeps production source intact.
            (p/'A04BR3PlatformExpert.cpp').write_text((ARM/'A04BR3PlatformExpert.cpp').read_text().replace('__asm__ volatile("dsb sy":::"memory");',''))
            (p/'test.cpp').write_text(host)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(p),'-I',str(ARM),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
            subprocess.run(['clang++','--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                            '-fno-exceptions','-fno-rtti','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096',
                            '-I',str(p),'-I',str(ARM),'-c',str(ARM/'A04BR3PlatformExpert.cpp'),'-o',str(p/'arm64.o')],check=True)

if __name__=='__main__':unittest.main()
