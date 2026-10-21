# SPDX-License-Identifier: MIT
"""Compile actual SDIO CPP for arm64. Declaration shim only; no XNU link."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDIO = Path(__file__).resolve().parents[2] / 'iokit/Kernel/arm/sdio'

class WLANDriverCompile(unittest.TestCase):
    def test_arm64_cpp_including_wmt_and_wlan_owned_boot_path(self):
        clang = shutil.which('clang++')
        self.assertIsNotNone(clang)
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            for name in ('IOKit', 'libkern/c++', 'libkern/crypto', 'pexpert/arm64'):
                (p / name).mkdir(parents=True, exist_ok=True)
            (p/'IOKit/IOLib.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
using vm_offset_t=uintptr_t;using vm_size_t=uintptr_t;
struct IOLock;
extern "C" IOLock *IOLockAlloc();extern "C" void IOLockFree(IOLock *);
extern "C" void IOLockLock(IOLock *);extern "C" void IOLockUnlock(IOLock *);
extern "C" void *IOMalloc(size_t);extern "C" void IOFree(void *,size_t);
extern "C" void bcopy(const void *,void *,size_t);
extern "C" void IOSleep(unsigned);extern "C" void IODelay(unsigned);
extern "C" void IOLog(const char *,...) __attribute__((format(printf,1,2)));
''')
            (p/'IOKit/IOService.h').write_text('''#pragma once
#include <IOKit/IOLib.h>
#define OSDeclareDefaultStructors(c)
#define OSDefineMetaClassAndStructors(c,b)
#define APPLE_KEXT_OVERRIDE override
#define OSDynamicCast(c,p) static_cast<c *>(p)
class OSObject {public:void release();};
class OSBoolean:public OSObject {};extern OSBoolean *kOSBooleanFalse,*kOSBooleanTrue;
class IOService:public OSObject {public:
 virtual bool start(IOService *);virtual void stop(IOService *);virtual void free();
 bool setProperty(const char *,OSBoolean *);bool setProperty(const char *,uint64_t,unsigned);
 OSObject *copyProperty(const char *);void registerService();
};
''')
            (p/'libkern/c++/OSData.h').write_text('''#pragma once
#include <IOKit/IOService.h>
class OSData:public OSObject {public:unsigned getLength();const void *getBytesNoCopy();};
''')
            (p/'libkern/crypto/sha2.h').write_text('''#pragma once
#define SHA256_DIGEST_LENGTH 32
struct SHA256_CTX {unsigned char opaque[256];};
extern "C" void SHA256_Init(SHA256_CTX *);
extern "C" void SHA256_Update(SHA256_CTX *,const void *,size_t);
extern "C" void SHA256_Final(void *,SHA256_CTX *);
''')
            (p/'pexpert/pexpert.h').write_text('''#pragma once
extern "C" int PE_parse_boot_argn(const char *,void *,int);
struct PE_state_t {void *bootArgs;};extern PE_state_t PE_state;
''')
            (p/'pexpert/device_tree.h').write_text('''#pragma once
using DTEntry=const void *;enum{kSuccess=1};
extern "C" int SecureDTLookupEntry(DTEntry,const char *,DTEntry *);
extern "C" int SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *);
''')
            (p/'pexpert/arm64/boot.h').write_text('''#pragma once
struct boot_args {uint64_t physBase,memSize,topOfKernelData;};
''')
            subprocess.run([clang, '--target=arm64-apple-darwin', '-std=c++14',
                            '-ffreestanding', '-fno-exceptions', '-fno-rtti', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-Wframe-larger-than=8192',
                            '-fstack-usage', '-I', str(p), '-c', str(SDIO/'A04BR3SDIO.cpp'),
                            '-o', str(p/'driver.o')], check=True)
            print((p/'driver.su').read_text(), end='')

if __name__ == '__main__':
    unittest.main()
