# SPDX-License-Identifier: MIT
"""Run the actual A04BR3 legacy SHA adapters across streaming boundaries."""
import ctypes
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest

class SHAVectors(unittest.TestCase):
    def test_legacy_adapters(self):
        root=Path(__file__).resolve().parents[2]/'libkern/crypto'
        sha1=(root/'corecrypto_sha1.c').read_text()
        sha1=sha1[sha1.index('static uint64_t'):sha1.index('/* This is not publicised')]
        sha2=(root/'corecrypto_sha2.c').read_text()
        sha2=sha2[sha2.index('#if defined(ARM64_BOARD_CONFIG_A04BR3)',sha2.index('SHA256_Final')):sha2.index('#else /* !ARM64_BOARD_CONFIG_A04BR3 */')]
        # Use the real SHA1 layout; SHA512 compile-time ABI fit is checked in XNU.
        header=(root.parent/'libkern/crypto/sha1.h').read_text()
        layout=header[header.index('typedef struct sha1_ctxt'):header.index('/* For compatibility')]
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);src=path/'adapters.c';lib=path/'sha.so'
            src.write_text('#include <stdint.h>\n#include <sys/types.h>\n#include <string.h>\n'+layout+'\ntypedef struct { uint64_t ctx[32]; } SHA512_CTX;\ntypedef SHA512_CTX SHA384_CTX;\n'+sha1+sha2+'\n#endif\n')
            bear=root/'a04br3_bearssl'
            inputs=[bear/f'src/{name}' for name in ('hash/sha1.c','hash/sha2big.c','codec/dec32be.c','codec/enc32be.c','codec/dec64be.c','codec/enc64be.c')]
            subprocess.run(['cc','-shared','-fPIC','-O2','-DARM64_BOARD_CONFIG_A04BR3','-I'+str(root),'-I'+str(bear/'inc'),'-I'+str(bear/'src'),str(src),*map(str,inputs),'-o',str(lib)],check=True)
            api=ctypes.CDLL(str(lib))
            for algo,prefix in (('sha1','SHA1'),('sha384','SHA384_'),('sha512','SHA512_')):
                init=getattr(api,prefix+'Init');update=getattr(api,prefix+'Update');final=getattr(api,prefix+'Final')
                init.argtypes=[ctypes.c_void_p];update.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t];final.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
                for data in (b'',b'abc',bytes(range(256))*100,b'Z'*1000000):
                    for step in (1,63,127,1024):
                        ctx=(ctypes.c_uint64*32)();init(ctx)
                        for at in range(0,len(data),step):
                            block=data[at:at+step];update(ctx,block,len(block))
                        digest=ctypes.create_string_buffer(hashlib.new(algo).digest_size);final(digest,ctx)
                        self.assertEqual(digest.raw,hashlib.new(algo,data).digest())
