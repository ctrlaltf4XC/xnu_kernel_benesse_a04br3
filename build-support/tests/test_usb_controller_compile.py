# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Compile actual bridge C/ARM64; shim declarations are not SDK validation."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class Controller(unittest.TestCase):
 def test_actual_arm64_translation_unit(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)
   headers={'sys/systm.h':'#include <stdint.h>\n#include <stddef.h>\nint printf(const char*,...);void delay(int);\n',
   'sys/errno.h':'#define EBUSY 16\n#define EIO 5\n#define ENOMEM 12\n#define EAGAIN 35\n#define EPERM 1\n',
   'vm/memory_types.h':'#include <stdint.h>\ntypedef uintptr_t vm_offset_t;typedef uintptr_t vm_size_t;\n',
   'machine/machine_routines.h':'',
   'kern/clock.h':'void delay(int);\n',
   'pexpert/pexpert.h':'int PE_parse_boot_argn(const char*,void*,int);\n'}
   for name,data in headers.items():
    f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(data)
   subprocess.run(['clang','--target=arm64-apple-ios16.0','-DARM64_BOARD_CONFIG_A04BR3','-std=c11','-ffreestanding','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wframe-larger-than=4096','-I',str(p),'-c',str(ROOT/'bsd/dev/a04br3_usb_controller.c'),'-o',str(p/'controller.o')],check=True)
if __name__=='__main__':unittest.main()
