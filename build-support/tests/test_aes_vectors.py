# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess
import tempfile
import unittest

class AESVectors(unittest.TestCase):
    def test_nist_cbc_vectors(self):
        root = Path(__file__).resolve().parents[2]
        provider = root / 'libkern/crypto/a04br3_bearssl'
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / 'aes-vectors'
            sources = [provider / 'src/symcipher' / (name + '.c') for name in
                       ('aes_ct', 'aes_ct_enc', 'aes_ct_dec', 'aes_ct_cbcenc', 'aes_ct_cbcdec')]
            subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra',
                            '-I' + str(provider / 'inc'), '-I' + str(provider / 'src'),
                            str(Path(__file__).with_name('aes_cbc_vectors.c')),
                            *map(str, sources), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
