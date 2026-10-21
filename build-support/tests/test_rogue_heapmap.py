# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Cold multi-heap GPU page walks, isolation and atomic rejection."""
from pathlib import Path
import subprocess
import tempfile
import unittest

GPU = Path(__file__).resolve().parents[2] / 'iokit/Kernel/arm/rogue'


class RogueHeapMap(unittest.TestCase):
    def test_independent_heaps_permissions_and_rejected_sets_preserve_tables(self):
        source = r'''
#include <assert.h>
#include <string.h>
#include "A04BR3RogueHeapMap.h"
alignas(4096) static unsigned char pc[4096],pd[3][4096],pt[3][8192],data[3][8192];
static unsigned char savedPC[4096],savedPD[sizeof(pd)],savedPT[sizeof(pt)];
static a04_rgx_bif regions[3];
static void reset() {
    const uint64_t va[]={0x65c01ff000ULL,0xda00001000ULL,0xe000200000ULL};
    for(unsigned n=0;n<3;n++)
        regions[n]={{pc,0x80000000,4096},{pd[n],0x80001000+n*0x10000,4096},
            {pt[n],0x80002000+n*0x10000,8192},{data[n],0x81000000+n*0x10000,8192},va[n],8192,n==2};
}
static void reject() {
    memcpy(savedPC,pc,sizeof(pc));memcpy(savedPD,pd,sizeof(pd));memcpy(savedPT,pt,sizeof(pt));
    assert(a04_rgx_heap_map_build(regions,3));
    assert(!memcmp(savedPC,pc,sizeof(pc)) && !memcmp(savedPD,pd,sizeof(pd)) && !memcmp(savedPT,pt,sizeof(pt)));
}
int main() {
    reset();memset(data,0x5a,sizeof(data));assert(!a04_rgx_heap_map_build(regions,3));
    /* Three different PCEs survive together, including a PT-boundary crossing. */
    for(unsigned n=0;n<3;n++) {
        uint64_t pa=0;
        for(unsigned at=0;at<8192;at++) {
            assert(!a04_rgx_bif_translate(&regions[n],regions[n].va+at,&pa));
            assert(pa==regions[n].data.pa+at && data[n][at]==0x5a);
        }
        assert(a04_rgx_bif_translate(&regions[n],regions[n].va-1,&pa));
        assert(a04_rgx_bif_translate(&regions[n],regions[n].va+8192,&pa));
    }
    assert((((uint64_t *)pt[2])[0]&2)==2); // USC may be published read-only.
    assert(!(((uint64_t *)pt[1])[1]&2));
    assert(!((uint32_t *)pc)[0]); // No accidental low VA mapping.
    regions[1].va=regions[0].va+0x200000;reject();reset(); // same PCE
    regions[1].pc.pa+=4096;reject();reset();
    regions[1].pd=regions[0].pd;reject();reset();
    regions[2].data.pa=regions[1].pt.pa;reject();reset();
    regions[2].pt.cpu=regions[0].data.cpu;reject();reset();
    regions[2].map_bytes=12288;reject();reset();
    regions[0].pt.bytes=4096;reject();reset(); // boundary requires two PTs
    regions[2].va=0xfffffff000ULL;reject();reset(); // VA overflow
    assert(a04_rgx_heap_map_build(nullptr,3));
    assert(a04_rgx_heap_map_build(regions,0));
    assert(a04_rgx_heap_map_build(regions,5));
    /* Descriptor storage cannot be erased by construction itself. */
    memcpy(pc,regions,sizeof(regions));
    assert(a04_rgx_heap_map_build((const a04_rgx_bif *)pc,3));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'test.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I', str(GPU),
                            str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
            subprocess.run([str(p / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
