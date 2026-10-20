# SPDX-License-Identifier: MIT
"""Exercise the selected portable MD5 source used for HFS volume IDs."""
import ctypes
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest


class MD5Vectors(unittest.TestCase):
    def test_vectors_and_block_boundary_streaming(self):
        root = Path(__file__).resolve().parents[2] / 'libkern/crypto/a04br3_bearssl'
        with tempfile.TemporaryDirectory() as directory:
            lib = Path(directory)/'md5.so'
            subprocess.run(['cc','-shared','-fPIC','-O2','-I'+str(root/'inc'),
                '-I'+str(root/'src'),str(root/'src/hash/md5.c'),
                str(root/'src/codec/dec32le.c'),str(root/'src/codec/enc32le.c'),
                '-o',str(lib)],check=True)
            api=ctypes.CDLL(str(lib))
            api.br_md5_init.argtypes=[ctypes.c_void_p]
            api.br_md5_update.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
            api.br_md5_out.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
            for data in (b'',b'a',b'abc',bytes(range(256))*100,b'Z'*1000000):
                ctx=(ctypes.c_uint64*32)();api.br_md5_init(ctx)
                for at in range(0,len(data),63):
                    block=data[at:at+63];api.br_md5_update(ctx,block,len(block))
                digest=ctypes.create_string_buffer(16);api.br_md5_out(ctx,digest)
                self.assertEqual(digest.raw,hashlib.md5(data).digest())
