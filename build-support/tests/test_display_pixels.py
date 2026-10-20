"""Exercise the actual scanout fill against all four surfaces and guard words."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
class DisplayPixels(unittest.TestCase):
    def test_four_opaque_surfaces_and_bounds(self):
        source = r'''
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "a04br3_display_pixels.h"
int main(void) {
    const unsigned n = A04BR3_DISPLAY_BYTES / 4;
    uint32_t *allocation = malloc((n + 2UL) * 4);
    assert(allocation);
    for (unsigned i=0; i<n+2; i++) allocation[i]=0x12345678;
    a04br3_display_paint(allocation + 1);
    assert(allocation[0]==0x12345678 && allocation[n+1]==0x12345678);
    for (unsigned b=0;b<4;b++) {
        uint32_t *p = allocation+1+b*A04BR3_DISPLAY_PIXELS;
        assert(p[0]==0xffff0000 && p[267]==0xff00ff00 && p[534]==0xff0000ff);
        assert(p[24*800]==0xff000000 && p[A04BR3_DISPLAY_PIXELS-1]==0xff000000);
        for (unsigned i=0;i<A04BR3_DISPLAY_PIXELS;i++) assert((p[i]>>24)==255);
    }
    assert(A04BR3_DISPLAY_BYTES==16384000);
    assert(A04BR3_DISPLAY_BYTES <= 0x1000000);
    free(allocation);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'pixels.c';p.write_text(source);exe=Path(tmp)/'pixels'
            subprocess.run(['cc','-std=c11','-O2','-Wall','-Werror','-I',str(ROOT/'bsd/dev'),str(p),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
