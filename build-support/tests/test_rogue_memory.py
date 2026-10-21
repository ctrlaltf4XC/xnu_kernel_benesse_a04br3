# SPDX-License-Identifier: BSD-2-Clause-Patent
from pathlib import Path
import subprocess
import tempfile
import unittest


class RogueMemory(unittest.TestCase):
    def test_reservation_cleanup_and_cache_publication(self):
        inc = Path(__file__).resolve().parents[2] / 'iokit/Kernel/arm/rogue'
        # Compile the actual reservation implementation with instrumented IOKit
        # allocation/cache shims. Only the final ARM barrier is host-substituted.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'IOKit').mkdir()
            (root / 'arm').mkdir()
            (root / 'IOKit/IOBufferMemoryDescriptor.h').write_text(r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
using IOByteCount=uint64_t;
using vm_offset_t=uintptr_t;
static int kernel_task=0;
enum {kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,
kIOMemoryMapperNone=32,kIOReturnSuccess=0};
static int live, completes, flushes, allocations, fault, failed;
class IOBufferMemoryDescriptor {
public:
 void *data; uint64_t size,pa; int id;
 static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(int,unsigned flags,
     uint64_t size,uint64_t mask) {
  int id=++allocations;
  assert(flags==(3|16|32));
  assert(mask==(id==1?0xffff8000ULL:0xfffff000ULL));
  if(fault==1 && id==2)return nullptr;
  auto b=new IOBufferMemoryDescriptor;
  b->id=id;b->size=size;b->pa=0x80000000ULL+id*0x100000;
  b->data=malloc(size);memset(b->data,0xcc,size);++live;return b;
 }
 int prepare(){if(fault==2 && id==2){++failed;return 1;}return 0;}
 void complete(){++completes;}
 void release(){free(data);delete this;--live;}
 uint64_t getPhysicalSegment(int,IOByteCount *len) {
  *len=(fault==3 && id==2)?4096:size;
  if(fault==4 && id==2)return 0x100000000ULL;
  if(fault==6 && id==1)return pa+4096;
  if(fault==7 && id==2)return 0;
  return pa;
 }
 void *getBytesNoCopy(){return (fault==5 && id==2)?nullptr:data;}
};
''')
            (root / 'arm/caches_internal.h').write_text(r'''
static void flush_dcache(vm_offset_t ptr,unsigned count,int physical) {
 assert(!physical);for(unsigned n=0;n<count;n++)assert(!((char*)ptr)[n]);
 ++flushes;
}
''')
            header = (inc / 'A04BR3RogueMemory.h').read_text()
            header = header.replace('__asm__ volatile("dsb sy":::"memory");',
                                    '__asm__ volatile("":::"memory");')
            (root / 'A04BR3RogueMemory.h').write_text(header)
            (root / 'test.cpp').write_text(r'''
#include "A04BR3RogueMemory.h"
int main() {
 assert(a04_rgx_mips_physical_range(0xfffff000,4096,4096));
 assert(!a04_rgx_mips_physical_range(0xfffff000,8192,4096));
 assert(!a04_rgx_mips_physical_range(0,4096,4096));
 assert(!a04_rgx_mips_physical_range(0x80000000,4096,3));
 for(fault=0;fault<=7;fault++) {
  allocations=completes=flushes=failed=0;
  { A04BR3RogueMemory memory;
    assert(memory.reserve()==(fault==0));
    if(!fault)assert(live==3 && flushes==3);
    else assert(live==0);
    memory.reset();memory.reset();assert(!live);
    assert(completes==allocations-(fault==1?1:failed));
  }
 }
 allocations=0;fault=0;
 { A04BR3RogueMemory memory;assert(memory.reserve()); }
 assert(!live);
}
''')
            subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I', str(root),
                            '-I', str(inc), str(root / 'test.cpp'),
                            '-o', str(root / 'test')], check=True)
            subprocess.run([str(root / 'test')], check=True)
