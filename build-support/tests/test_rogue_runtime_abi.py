# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Regenerate/verify declaration-only MIT DDK ABI: run with --regenerate.

Original MT8167 user config is preprocessed verbatim; only declarations and
static ABI assertions are retained. No DDK functions, Linux runtime or binaries.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

KERNEL=Path(__file__).resolve().parents[2]
GPU=KERNEL/'iokit/Kernel/arm/rogue'
DDK=KERNEL.parent.parent.parent/'port/gsi/kernel-4.14/drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693'


def extract(text):
    text=text[text.index('typedef unsigned int IMG_UINT'):]
    lines=text.splitlines();blocks=[];i=0
    while i<len(lines):
        if lines[i].lstrip().startswith(('typedef ','_Static_assert')):
            part=[];depth=0
            while i<len(lines):
                line=lines[i];part.append(line);depth+=line.count('{')-line.count('}');i+=1
                if not depth and line.rstrip().endswith(';'):break
            blocks.append('\n'.join(part))
        else:i+=1
    return '\n'.join(blocks).replace('_Static_assert','A04_DDK_ASSERT')+'\n'


def original_declarations(work):
    src=work/'source.c';src.write_text('#include "rgx_fwif_km.h"\n#include "rgx_fwif_hwperf.h"\n')
    dirs=sorted({p.parent for p in DDK.rglob('*.h')}|{DDK/'hwdefs',DDK/'include/public'})
    return subprocess.check_output(['cc','-std=gnu11','-include',str(DDK/'include/config_kernel_user_mt8167.h'),
        *[value for d in dirs for value in ('-I',str(d))],'-E','-P',str(src)],text=True)


def regenerate(work):
    license=(DDK/'services/include/rgx_fwif_km.h').read_text().split('#if')[0]
    return '''/* Generated declarations from Imagination/MediaTek m1.11ED5425693,
 * config_kernel_user_mt8167.h, rgx_fwif_km.h, rgx_fwif_hwperf.h and dependencies.
 * Original MIT license retained below. No Linux functions or runtime imported.
 * Regenerate/compare with build-support/tests/test_rogue_runtime_abi.py.
 */
'''+license+'''
#ifndef A04BR3_ROGUE_DDK_ABI_H
#define A04BR3_ROGUE_DDK_ABI_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
#define A04_DDK_ASSERT static_assert
#else
#define A04_DDK_ASSERT _Static_assert
#endif
'''+extract(original_declarations(work))+'''#undef A04_DDK_ASSERT
#endif
'''


class RuntimeABI(unittest.TestCase):
    def test_generated_declarations_match_actual_ddk(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual((GPU/'A04BR3RogueDDKABI.h').read_text(),regenerate(Path(directory)))

    def test_wire_abi_on_host_arm64_and_mips32(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);source=work/'abi.c'
            source.write_text('''#include "A04BR3RogueDDKABI.h"
_Static_assert(sizeof(RGXFWIF_INIT)==592,"INIT");
_Static_assert(offsetof(RGXFWIF_INIT,bFirmwareStarted)==260,"ready");
_Static_assert(sizeof(RGXFWIF_TRACEBUF)==4152,"trace");
_Static_assert(sizeof(RGXFWIF_HWRINFOBUF)==2072,"HWR");
_Static_assert(sizeof(RGXFWIF_RUNTIME_CFG)==24,"runtime");
_Static_assert(sizeof(RGXFWIF_OS_CONFIG)==20,"config");
_Static_assert(sizeof(RGXFWIF_KCCB_CMD)==56,"KCCB");
_Static_assert(sizeof(RGXFWIF_FWCCB_CMD)==40,"FWCCB");
_Static_assert(sizeof(RGXFWIF_CCB_CTL)==16,"CCBCTL");
int main(void){return 0;}
''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(GPU),
                            str(source),'-o',str(work/'abi')],check=True)
            subprocess.run([str(work/'abi')],check=True)
            clang=shutil.which('clang')
            if not clang:self.fail('clang required for exact target ABI checks')
            for target in ('arm64-apple-darwin','mipsel-none-elf'):
                subprocess.run([clang,'--target='+target,'-ffreestanding','-std=c11',
                                '-I',str(GPU),'-fsyntax-only',str(source)],check=True)


if __name__=='__main__':
    if sys.argv[1:]==['--regenerate']:
        with tempfile.TemporaryDirectory() as directory:
            (GPU/'A04BR3RogueDDKABI.h').write_text(regenerate(Path(directory)))
    else:unittest.main()
