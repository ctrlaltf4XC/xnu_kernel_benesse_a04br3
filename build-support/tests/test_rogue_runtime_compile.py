# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Compile the production GPU CPP and runtime with declaration-only IOKit shim.
Not a full XNU link. Original ARM barriers/cache instructions remain unchanged.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

GPU=Path(__file__).resolve().parents[2]/'iokit/Kernel/arm/rogue'


class RuntimeCompile(unittest.TestCase):
    def test_arm64_full_driver_runtime_and_original_cache_instructions(self):
        clang=shutil.which('clang++')
        if not clang:self.fail('clang++ needed for target compile')
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)
            for d in ('IOKit','arm','pexpert/arm64'): (p/d).mkdir(parents=True,exist_ok=True)
            (p/'IOKit/IOLib.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
using vm_offset_t=uintptr_t;using vm_size_t=uintptr_t;
extern "C" void bzero(void *,size_t);
extern "C" void bcopy(const void *,void *,size_t);
extern "C" void IODelay(unsigned);
extern "C" void IOLog(const char *,...) __attribute__((format(printf,1,2)));
''')
            (p/'IOKit/IOLocks.h').write_text('struct IOLock {};\nstatic IOLock *IOLockAlloc(){return new IOLock;}\nstatic void IOLockFree(IOLock *p){delete p;}\nstatic void IOLockLock(IOLock *){}\nstatic bool IOLockTryLock(IOLock *){return true;}\nstatic void IOLockUnlock(IOLock *){}\n')
            (p/'arm').mkdir(exist_ok=True)
            (p/'arm/machine_routines.h').write_text('static bool inInterrupt;\nstatic bool ml_at_interrupt_context(){return inInterrupt;}\n')
            (p/'IOKit/IOService.h').write_text('''#pragma once
#define OSDeclareDefaultStructors(c)
#define OSDefineMetaClassAndStructors(c,b)
#define APPLE_KEXT_OVERRIDE override
struct OSBoolean;extern OSBoolean *kOSBooleanFalse,*kOSBooleanTrue;
using IOReturn=int;
 enum {kIOReturnSuccess=0,kIOReturnBadArgument=1,kIOReturnNotPermitted=2,kIOReturnTimeout=3,
       kIOReturnBusy=4,kIOReturnNotReady=5,kIOReturnError=6};
 class OSSymbol {public:bool isEqualTo(const char *)const;};
 class IOService {public:
 virtual void free();
 virtual IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *);
 virtual ~IOService();virtual bool start(IOService *);virtual void stop(IOService *);
 bool setProperty(const char *,const char *);
 bool setProperty(const char *,OSBoolean *);
 bool setProperty(const char *,uint64_t,unsigned);
 void registerService();
};
''')
            (p/'IOKit/IOBufferMemoryDescriptor.h').write_text('''#pragma once
#include <IOKit/IOLib.h>
using IOByteCount=uint64_t;
extern void *kernel_task;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMemoryMapperNone=32,kIOMapWriteCombineCache=0x400};
class IOBufferMemoryDescriptor {public:
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(void *,unsigned,uint64_t,uint64_t);
 int prepare();void complete();void release();
 uint64_t getPhysicalSegment(int,IOByteCount *);void *getBytesNoCopy();
};
''')
            (p/'arm/caches_internal.h').write_text('extern "C" void flush_dcache(vm_offset_t,unsigned,int);\n')
            (p/'pexpert/pexpert.h').write_text('''extern "C" int PE_parse_boot_argn(const char *,void *,int);
struct PE_state_t {void *bootArgs;};extern PE_state_t PE_state;
''')
            (p/'pexpert/device_tree.h').write_text('''using DTEntry=const void *;
enum {kSuccess=1};
extern "C" int SecureDTLookupEntry(DTEntry,const char *,DTEntry *);
extern "C" int SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *);
''')
            (p/'pexpert/arm64/boot.h').write_text('struct boot_args {uint64_t physBase,memSize,topOfKernelData;};\n')
            subprocess.run([clang,'--target=arm64-apple-darwin','-std=c++14','-ffreestanding',
                '-fno-exceptions','-fno-rtti','-O2','-Wall','-Wextra','-Werror','-Wframe-larger-than=8192',
                '-fstack-usage','-I',str(p),'-c',str(GPU/'A04BR3RogueGPU.cpp'),
                '-o',str(p/'driver.o')],check=True)
            print((p/'driver.su').read_text(),end='')
