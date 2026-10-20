"""Evaluate the actual compressor policy branches against the board VA budget."""
import pathlib
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[2] / 'osfmk/vm/vm_compressor.c'

class CompressorBudget(unittest.TestCase):
    def test_embedded_and_desktop_policies(self):
        source = SOURCE.read_text()
        start = source.index('#if XNU_TARGET_OS_OSX && !defined(A04BR3)')
        end = source.index('\n\tc_seg_max_pages =', start)
        policy = source[start:end]
        for board in (False, True):
            program = ('#include <stdint.h>\n#include <stdio.h>\n'
                       '#define XNU_TARGET_OS_OSX 1\n'
                       '#define VM_COMPRESSOR_MAX_POOL_SIZE (192ULL << 30)\n'
                       + ('#define A04BR3 1\n' if board else '') +
                       'int main(void) { uint64_t vm_compression_limit=0;'
                       'uint64_t compressor_pool_size=0, compressor_pool_max_size=UINT64_MAX;'
                       'uint64_t compressor_pool_multiplier=0;'
                       'uint64_t max_mem=1504ULL<<20;\n' + policy +
                       '\n(void)compressor_pool_multiplier; printf("%llu", (unsigned long long)compressor_pool_size); }\n')
            with tempfile.TemporaryDirectory() as tmp:
                c = pathlib.Path(tmp)/'policy.c'; c.write_text(program)
                exe = pathlib.Path(tmp)/'policy'
                subprocess.run(['cc','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
                pool = int(subprocess.check_output([str(exe)]))
            self.assertEqual(pool, (1504<<20) * (1 if board else 16))
            # Static zone + IOKit + compressor, before additional metadata.
            claims = (24<<30) + (2<<30) + pool
            if board:
                self.assertLess(claims, 48<<30)
            else:
                self.assertGreater(claims, 48<<30)

if __name__ == '__main__':
    unittest.main()
