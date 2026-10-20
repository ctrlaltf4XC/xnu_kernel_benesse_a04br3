# SPDX-License-Identifier: MIT
"""Run the actual raw-reader loop with faulting uio and sector fixtures."""
from pathlib import Path
import subprocess
import tempfile
import unittest


class RawReaderTests(unittest.TestCase):
    def test_offsets_eof_and_copy_faults(self):
        source=(Path(__file__).resolve().parents[2]/'bsd/dev/a04br3_msdc.c').read_text()
        reader=source[source.index('static int disk_read('):source.index('static int disk_write(')]
        harness=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#define __unused __attribute__((unused))
#define dev_t int
#define caddr_t char *
#define MIN(a,b) ((a)<(b)?(a):(b))
struct uio { int64_t off,resid; unsigned moved; int fault; unsigned char data[4096]; };
static uint32_t disk_sectors=4,root_first=1,root_sectors=2;
#define minor(d) (d)
static int fail_block=-1;
#define uio_offset(u) ((u)->off)
#define uio_resid(u) ((u)->resid)
static int read_disk_sector(uint32_t block,uint8_t data[512]) {
    if ((int)block==fail_block) return EIO;
    for (unsigned i=0;i<512;i++) data[i]=(unsigned char)(block*512+i);
    return 0;
}
static int uiomove(char *data,int count,struct uio *u) {
    if (u->fault) return EFAULT;
    assert(u->moved+count<=sizeof(u->data));
    memcpy(u->data+u->moved,data,count);u->moved+=count;u->resid-=count;u->off+=count;
    return 0;
}
'''
        harness+=reader+r'''
int main(void) {
    struct uio u={.off=512,.resid=512};
    assert(disk_read(0,&u,0)==0 && u.moved==512);
    for (unsigned i=0;i<512;i++) assert(u.data[i]==(unsigned char)(512+i));
    u=(struct uio){.off=511,.resid=1026};
    assert(disk_read(0,&u,0)==0 && u.moved==1026);
    for (unsigned i=0;i<1026;i++) assert(u.data[i]==(unsigned char)(511+i));
    u=(struct uio){.off=2040,.resid=16};
    assert(disk_read(0,&u,0)==0 && u.moved==8 && u.resid==8);
    u=(struct uio){.off=2048,.resid=512};
    assert(disk_read(0,&u,0)==0 && u.moved==0);
    u=(struct uio){.off=-1,.resid=1}; assert(disk_read(0,&u,0)==EINVAL);
    u=(struct uio){.off=512,.resid=512,.fault=1};
    assert(disk_read(0,&u,0)==EFAULT && u.moved==0);
    u=(struct uio){.off=1016,.resid=16};
    assert(disk_read(1,&u,0)==0 && u.moved==8 && u.resid==8);
    fail_block=1;u=(struct uio){.off=512,.resid=512};
    assert(disk_read(0,&u,0)==EIO && u.moved==0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            c=Path(directory)/'reader.c';exe=Path(directory)/'reader';c.write_text(harness)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
