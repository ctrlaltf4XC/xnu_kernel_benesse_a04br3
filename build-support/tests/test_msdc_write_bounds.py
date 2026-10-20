# SPDX-License-Identifier: MIT
"""Execute the actual final hardware-write gate with adversarial LBAs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class WriteBounds(unittest.TestCase):
    def test_final_gate(self):
        source=(Path(__file__).resolve().parents[2]/'bsd/dev/a04br3_msdc.c').read_text()
        gate=source[source.index('static int write_root_block('):source.index('static int flush_card(')]
        harness=r'''
#include <stdint.h>
#include <errno.h>
#include <assert.h>
static unsigned write_enabled;
static uint32_t disk_sectors=100,root_first=40,root_sectors=20;
static unsigned writes,last_lba;
static int failure;
static int write_block_raw(uint32_t block,const uint8_t *data) {
    assert(data);writes++;last_lba=block;return failure;
}
'''+gate+r'''
int main(void) {
    uint8_t data[512]={0};
    assert(write_root_block(0,data)==EROFS && writes==0);
    write_enabled=1;
    assert(write_root_block(0,data)==0 && writes==1 && last_lba==40);
    assert(write_root_block(19,data)==0 && writes==2 && last_lba==59);
    assert(write_root_block(20,data)==EINVAL && writes==2);
    assert(write_root_block(UINT64_MAX,data)==EINVAL && writes==2);
    root_sectors=100;
    assert(write_root_block(60,data)==EINVAL && writes==2);
    root_first=UINT32_MAX;
    assert(write_root_block(0,data)==EINVAL && writes==2);
    root_first=40;root_sectors=20;failure=EIO;
    assert(write_root_block(0,data)==EIO && writes==3);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'bounds.c';exe=Path(d)/'bounds';source.write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror',str(source),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
