# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Exercise the actual memory stage API with owned-buffer IOKit shims.

No firmware blob is stored in the repo. Synthetic ELF/info fixtures and all
binaries are temporary; a real blob is read from A04_RGX_FIRMWARE or board path.
Only the ARM barrier is substituted; production parser/MMU/stage code is used.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

KERNEL = Path(__file__).resolve().parents[2]
HEADER = KERNEL / 'iokit/Kernel/arm/rogue'
ROOT = KERNEL.parent.parent.parent


def synthetic():
    b = bytearray(8192)
    struct.pack_into('<16sHHIIIIIHHHHHH', b, 0,
                     b'\x7fELF\x01\x01\x01'+bytes(9), 2, 8, 1,
                     0xbfc00001, 52, 0, 0, 52, 32, 5, 0, 0, 0)
    for i, (address, source, filesz, memsz, permissions) in enumerate((
            (0x9fc02000, 256, 4, 8, 5), (0xbfc00000, 260, 4, 8, 5),
            (0xc0000000, 264, 4, 8, 5), (0xc0032000, 268, 0, 8, 6),
            (0xc0032400, 268, 4, 8, 6))):
        struct.pack_into('<8I', b, 52+i*32, 1, source, address, 0,
                         filesz, memsz, permissions, 4)
    b[256:272] = b'EXCPBOOTCODEDATA'
    struct.pack_into('<4IQI', b, 4096, 1, 32, 6, 24, 0x001600280036001e, 0)
    for i, section in enumerate((
            (4, 1, 0xc0000000, 0x2f000, 0x17000, 0),
            (5, 1, 0x9fc02000, 0x2000, 0x1000, 0x17000),
            (6, 1, 0xbfc00000, 0x1000, 0x1000, 0x18000),
            (7, 2, 0xc0032000, 0x8000, 0x3000, 0),
            (8, 2, 0xbfc01000, 0x1000, 0x1000, 0x3000),
            (9, 2, 0xcf600000, 0x1000, 0x1000, 0x4000))):
        struct.pack_into('<6I', b, 4128+i*24, *section)
    return b


class FirmwareStage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.work = Path(cls.tmp.name)
        cls.cxx = shutil.which('c++') or shutil.which('clang++')
        spec = importlib.util.spec_from_file_location(
            'stage_reference', KERNEL.parent/'device/tools/rogue_firmware.py')
        cls.ref = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.ref)
        (cls.work/'IOKit').mkdir()
        (cls.work/'arm').mkdir()
        (cls.work/'IOKit/IOBufferMemoryDescriptor.h').write_text(r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
using IOByteCount=uint64_t;
using vm_offset_t=uintptr_t;
static int kernel_task=0;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,
      kIOMemoryMapperNone=32,kIOReturnSuccess=0};
class IOBufferMemoryDescriptor;
static IOBufferMemoryDescriptor *slots[3];
static int live,allocations,completed,flushes,barriers,fault;
static uint64_t physicalBase;
static void a04_test_barrier();
class IOBufferMemoryDescriptor {
public:
 void *data; uint64_t size,pa; unsigned id;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(int,unsigned flags,
     uint64_t size,uint64_t mask) {
  unsigned id=(unsigned)allocations++%3;
  assert(!slots[id]);assert(flags==(3|16|32));
  assert(mask==(id==0?0xffff8000ULL:0xfffff000ULL));
  auto b=new IOBufferMemoryDescriptor;
  b->id=id;b->size=size;b->pa=physicalBase+(id+1)*0x100000ULL;
  b->data=malloc(size);assert(b->data);memset(b->data,0xcc,size);
  slots[id]=b;++live;return b;
 }
 int prepare(){return 0;}
 void complete(){++completed;}
 void release(){slots[id]=nullptr;free(data);delete this;--live;}
 uint64_t getPhysicalSegment(int,IOByteCount *len) {
  *len=(fault==3 && id==1)?4096:size;
  if(fault==1 && id==0)return pa+4096;
  if(fault==2 && id==1)return 0x100000000ULL;
  if(fault==4 && id==1)return slots[0]->pa+4096;
  if(fault==5 && id==2)return 0xfffff000ULL;
  if(fault==6 && id==1)return 0;
  return pa;
 }
 void *getBytesNoCopy(){
  if(fault==7 && id==1)return slots[0]->data;
  if(fault==8 && id==2)return nullptr;
  if(fault==9 && id==0)return (char*)data+1;
  if(fault==10 && id==2)return (void *)(uintptr_t)-16;
  return data;
 }
};
''')
        (cls.work/'arm/caches_internal.h').write_text(
            'static void flush_dcache(vm_offset_t,unsigned,int);\n')
        memory = (HEADER/'A04BR3RogueMemory.h').read_text()
        memory = memory.replace('__asm__ volatile("dsb sy":::"memory");',
                                'a04_test_barrier();')
        (cls.work/'A04BR3RogueMemory.h').write_text(memory)
        source = cls.work/'stage.cpp'
        source.write_text(r'''
#include <vector>
#include <fstream>
#include <iterator>
#include <stdio.h>
#include "A04BR3RogueMemory.h"
static A04BR3RogueMemory *current;
static bool checkStage;
static unsigned cleanOrder;
static std::vector<unsigned char> expected[3];
static std::vector<unsigned char> readFile(const char *name) {
 std::ifstream f(name,std::ios::binary);assert(f.good());
 return std::vector<unsigned char>(std::istreambuf_iterator<char>(f),{});
}
static void flush_dcache(vm_offset_t ptr,unsigned count,int physical) {
 assert(!physical && current && !current->firmwareStaged());
 unsigned i=0;for(;i<3;i++)if(slots[i] && slots[i]->data==(void *)ptr)break;
 assert(i<3 && count==slots[i]->size);
 if(checkStage) {
  const unsigned order[3]={1,2,0};assert(cleanOrder<3 && i==order[cleanOrder++]);
  assert(expected[i].size()==count);
  assert(!memcmp((void *)ptr,expected[i].data(),count));
 } else for(unsigned n=0;n<count;n++)assert(!((unsigned char *)ptr)[n]);
 ++flushes;
}
static void a04_test_barrier() {
 assert(current && !current->firmwareStaged());
 if(checkStage)assert(cleanOrder==3);
 ++barriers;
}
static void reserve(A04BR3RogueMemory &memory) {
 checkStage=false;fault=0;assert(memory.reserve());assert(live==3);
 assert(!memory.firmwareStaged());
}
static void success(A04BR3RogueMemory &memory,const std::vector<unsigned char> &blob) {
 checkStage=true;cleanOrder=0;int oldFlush=flushes,oldBarriers=barriers;
 /* Force unaligned source; the API must not cast ELF or trailer records. */
 std::vector<unsigned char> unaligned(blob.size()+1);
 memcpy(unaligned.data()+1,blob.data(),blob.size());
 assert(memory.stage(unaligned.data()+1,blob.size())==A04BR3RogueMemory::StageOK);
 assert(memory.firmwareStaged() && live==3);
 assert(!memcmp(unaligned.data()+1,blob.data(),blob.size()));
 assert(flushes-oldFlush==3 && barriers-oldBarriers==1 && cleanOrder==3);
 for(unsigned i=0;i<3;i++)assert(!memcmp(slots[i]->data,expected[i].data(),expected[i].size()));
 checkStage=false;
}
int main(int argc,char **argv) {
 assert(argc==8);
 const auto blob=readFile(argv[2]);
 expected[1]=readFile(argv[3]);expected[2]=readFile(argv[4]);expected[0]=readFile(argv[5]);
 physicalBase=strtoull(argv[6],nullptr,0);
 A04BR3RogueMemory memory;current=&memory;
 assert(!memory.firmwareStaged());
 assert(memory.stage(blob.data(),blob.size())==A04BR3RogueMemory::StageNoMemory);
 assert(!live && !memory.firmwareStaged());
 reserve(memory);success(memory,blob);
 if(!strcmp(argv[1],"success")) {
  /* Rebuild from scratch: populated old PTEs must not cause overlap errors. */
  success(memory,blob);
  memory.reset();assert(!live && !memory.firmwareStaged());
  reserve(memory);success(memory,blob);
  assert(memory.stage(nullptr,blob.size())!=0);
  assert(!live && !memory.firmwareStaged());
 } else if(!strcmp(argv[1],"reject")) {
  auto bad=readFile(argv[7]);int before=flushes;
  assert(memory.stage(bad.data(),bad.size())!=0);
  assert(!live && !memory.firmwareStaged() && before==flushes);
  reserve(memory);success(memory,blob);
 } else if(!strcmp(argv[1],"faults")) {
  for(int f=1;f<=10;f++) {
   int before=flushes;fault=f;
   assert(memory.stage(blob.data(),blob.size())==A04BR3RogueMemory::StageAddress);
   assert(!live && !memory.firmwareStaged() && before==flushes);
   reserve(memory);success(memory,blob);
  }
  for(unsigned i=0;i<3;i++) {
   const void *alias=(char*)slots[i]->data+1;int before=flushes;
   assert(memory.stage(alias,4097)==A04BR3RogueMemory::StageAlias);
   assert(!live && !memory.firmwareStaged() && before==flushes);
   reserve(memory);success(memory,blob);
  }
 } else assert(!"unknown mode");
 memory.reset();assert(!live && !memory.firmwareStaged());
 assert(completed==allocations);
 puts("PASS: owned-memory stage/clean/failure/reset; no GPU hardware access");
}
''')
        cls.exe = cls.work/'stage'
        subprocess.run([cls.cxx, '-std=c++14', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I', str(cls.work), '-I', str(HEADER), str(source),
                        '-o', str(cls.exe)], check=True)

    def run_stage(self, mode, blob, physical_base=0x80000000, bad=None):
        _, buffers = self.ref.stage(blob)
        table_pa, code_pa, data_pa = [physical_base+i*0x100000 for i in (1, 2, 3)]
        struct.pack_into('<4Q', buffers[2], 0x3000,
                         0x13000000, table_pa, data_pa+0x4000, 0)
        table = bytearray(32768)
        for offset, pa, count, flags in ((0, code_pa, 25, 0x13),
                                          (0x32000, data_pa, 5, 0x40000017)):
            for n in range(count):
                struct.pack_into('<I', table, (offset//4096+n)*4,
                                 ((pa+n*4096)>>6)|flags)
        paths = [self.work/name for name in ('blob.bin', 'code.bin', 'data.bin', 'table.bin')]
        for path, data in zip(paths, (blob, buffers[1], buffers[2], table)):
            path.write_bytes(data)
        bad_path = self.work/'bad.bin'
        bad_path.write_bytes(bad if bad is not None else b'')
        subprocess.run([str(self.exe), mode, *map(str, paths), hex(physical_base),
                        str(bad_path)], check=True)

    def test_arm64_original_header_compiles_without_stage_driver(self):
        clang = shutil.which('clang++')
        if not clang:
            self.skipTest('clang++ required for ARM64 cross compilation')
        cross = self.work/'cross'
        (cross/'IOKit').mkdir(parents=True)
        (cross/'arm').mkdir()
        (cross/'IOKit/IOBufferMemoryDescriptor.h').write_text(r'''
#include <stdint.h>
#include <stddef.h>
using IOByteCount=uint64_t;
using vm_offset_t=uintptr_t;
extern void *kernel_task;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,
      kIOMemoryMapperNone=32,kIOReturnSuccess=0};
extern "C" void bzero(void *,size_t);
class IOBufferMemoryDescriptor {
public:
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(void *,unsigned,uint64_t,uint64_t);
 int prepare();void complete();void release();
 uint64_t getPhysicalSegment(int,IOByteCount *);
 void *getBytesNoCopy();
};
''')
        (cross/'arm/caches_internal.h').write_text(
            'extern "C" void flush_dcache(vm_offset_t,unsigned,int);\n')
        (cross/'compile.cpp').write_text('''
#include "A04BR3RogueMemory.h"
int test(A04BR3RogueMemory *memory,const void *blob,size_t size) {
 return memory->stage(blob,size);
}
bool reserve(A04BR3RogueMemory *memory) { return memory->reserve(); }
''')
        subprocess.run([clang, '--target=arm64-apple-darwin', '-std=c++14',
                        '-ffreestanding', '-fno-exceptions', '-fno-rtti', '-O2',
                        '-Wall', '-Wextra', '-Werror', '-Wframe-larger-than=4096',
                        '-fstack-usage', '-I', str(cross), '-I', str(HEADER),
                        '-c', str(cross/'compile.cpp'), '-o', str(cross/'compile.o')], check=True)
        print((cross/'compile.su').read_text(), end='')

    def test_profile_stage_pte_boot_cache_order_and_retry(self):
        self.run_stage('success', synthetic())
        # Changing the owned allocations must change PTEs and boot words.
        self.run_stage('success', synthetic(), physical_base=0x90000000)

    def test_invalid_and_wrong_profile_after_success_discards_old_candidate(self):
        blob = synthetic()
        mutations = ((4096, 2), (4112, 0), (52+4, 4095),
                     (4128+2*24+20, 0x19000),  # changed allocation total
                     (4128+5*24+20, 0x5000),
                     (4128+12, 0x30000),  # same totals, wrong maximum
                     (4128+3*24+16, 0x2000),  # same totals, wrong allocated
                     (4128+4*24+8, 0xbfc11000),  # same totals, wrong base
                     (24, 0xc0000001),  # valid ELF entry, unsupported profile
                     (52+24, 7), (52+3*32+24, 7),  # unexpected permissions
                     (52+32+16, 0))  # entry exists only in BSS
        for offset, value in mutations:
            bad = bytearray(blob)
            struct.pack_into('<I', bad, offset, value)
            with self.subTest(offset=offset, value=value):
                self.run_stage('reject', blob, bad=bad)
        self.run_stage('reject', blob, bad=blob[:4096])

    def test_invalid_physical_cpu_ranges_and_blob_alias_fail_closed(self):
        self.run_stage('faults', synthetic())

    def test_external_real_firmware_matches_reference_pte_and_boot_bytes(self):
        path = Path(os.environ.get('A04_RGX_FIRMWARE', str(
            ROOT/'port/gsi/vendor/benesse/a04br3/proprietary/vendor/firmware/rgx.fw.22.40.54.30')))
        if not path.is_file():
            if 'A04_RGX_FIRMWARE' in os.environ:
                self.fail('explicit external firmware file is missing')
            self.skipTest('external board firmware not present')
        self.run_stage('success', path.read_bytes())


if __name__ == '__main__':
    unittest.main()
