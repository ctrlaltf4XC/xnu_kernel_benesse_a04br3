# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Actual kernel BearSSL + PersonalCrypto adapter, not replacement crypto mocks.
Requires cc/clang, ld.lld, nm, openssl CLI and Python standard library only.
Run standalone: python3 -m unittest discover -s build-support/tests -p test_wlan_personal_crypto.py -v
"""
import ctypes as C
import hashlib
import hmac
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BEAR = ROOT / 'libkern/crypto/a04br3_bearssl'
SOURCES = ['hash/sha1.c', 'codec/dec32be.c', 'codec/enc32be.c',
           'symcipher/aes_ct.c', 'symcipher/aes_ct_dec.c', 'symcipher/aes_ct_cbcdec.c']
WRAPPER = r'''
#include "iokit/Kernel/arm/sdio/A04BR3WLANPersonalCrypto.h"
int pmk(const uint8_t *p,unsigned pn,const uint8_t *s,unsigned sn,uint8_t *o){return a04_wlan_personal_pmk(p,pn,s,sn,o);}
int ptk(const uint8_t *p,const uint8_t *a,const uint8_t *s,const uint8_t *an,const uint8_t *sn,uint8_t *o){return a04_wlan_personal_ptk(p,a,s,an,sn,o);}
int mac(const uint8_t *k,unsigned kn,const uint8_t *p,unsigned n,uint8_t *o){return a04_wlan_personal_hmac_sha1(k,kn,p,n,o);}
int mic(const uint8_t *k,const uint8_t *p,unsigned n,uint8_t *o){return a04_wlan_personal_mic(k,p,n,o);}
int verify(const uint8_t *k,const uint8_t *p,unsigned n){return a04_wlan_personal_mic_verify(k,p,n);}
int unwrap(const uint8_t *k,const uint8_t *p,unsigned n,uint8_t *o,unsigned cap,unsigned *len){return a04_wlan_personal_unwrap(k,p,n,o,cap,len);}
int equal(const uint8_t *a,const uint8_t *b,unsigned n){return a04_wlan_personal_equal(a,b,n);}
'''
SANITIZER_MAIN = r'''
#include <assert.h>
int main(void) {
 uint8_t key[32]={0},out[2050],p[2048]={0},a[6]={0},nonce[32]={0};unsigned len=99;
 memset(out,0xcc,sizeof(out));
 assert(pmk((const uint8_t *)"password",8,(const uint8_t *)"IEEE",4,out+1)==0);
 assert(out[0]==0xcc && out[33]==0xcc);
 assert(ptk(key,a,a,nonce,nonce,out+1)==0 && out[65]==0xcc);
 assert(pmk(NULL,8,p,1,out)==1);assert(pmk(p,64,p,1,out)==1);
 assert(mac(key,32,p,2049,out)==1);assert(mac(NULL,0,NULL,0,out)==0);
 assert(unwrap(key,p,24,out+1,2048,&len)==2 && len==0);
 assert(unwrap(key,p,24,out+1,15,&len)==1);
 assert(unwrap(key,p,24,out,2048,NULL)==1);
 for(unsigned n=0;n<2048;n++) {
  p[0]=2;p[1]=3;p[4]=2;p[6]=2;
  p[2]=(uint8_t)((n>=4?n-4:0)>>8);p[3]=(uint8_t)(n>=4?n-4:0);
  p[97]=(uint8_t)((n>=99?n-99:0)>>8);p[98]=(uint8_t)(n>=99?n-99:0);
  int e=mic(key,p,n,out);assert(e==(n>=99?0:1));
 }
 for(unsigned n=0;n<=2049;n++) {
  int e=unwrap(key,p,n,out,2048,&len);
  assert(e!=0 && len==0);
 }
 for(unsigned n=0;n<16;n++) {uint8_t b[16]={0};b[n]=1;assert(!equal(key,b,16));}
 assert(equal(key,key,16));return 0;
}
'''

class PersonalCrypto(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.path = Path(cls.tmp.name)
        cls.src = cls.path/'adapter.c'
        cls.src.write_text(WRAPPER)
        cls.inputs = [str(BEAR/'src'/s) for s in SOURCES]
        cls.flags = ['-I'+str(ROOT), '-I'+str(BEAR/'inc'), '-I'+str(BEAR/'src')]
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        *cls.flags, str(cls.src), *cls.inputs, '-o', str(cls.path/'crypto.so')], check=True)
        cls.api = C.CDLL(str(cls.path/'crypto.so'))
        pointer = C.c_void_p
        signatures = {'pmk':[pointer,C.c_uint,pointer,C.c_uint,pointer],
                      'ptk':[pointer]*6, 'mac':[pointer,C.c_uint,pointer,C.c_uint,pointer],
                      'mic':[pointer,pointer,C.c_uint,pointer], 'verify':[pointer,pointer,C.c_uint],
                      'unwrap':[pointer,pointer,C.c_uint,pointer,C.c_uint,C.POINTER(C.c_uint)],
                      'equal':[pointer,pointer,C.c_uint]}
        for name,args in signatures.items():
            getattr(cls.api,name).argtypes=args
            getattr(cls.api,name).restype=C.c_int

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    @staticmethod
    def output(n):
        return C.create_string_buffer(b'\xcc'*n,n)

    def test_provider_provenance(self):
        metadata=json.loads((ROOT/'iokit/Kernel/arm/sdio/A04BR3WLANPersonalCrypto.sources.json').read_text())
        for path,expected in metadata['provider']['sha256'].items():
            self.assertEqual(hashlib.sha256((ROOT/path).read_bytes()).hexdigest(),expected,path)

    def test_pmk_known_and_cross_implementation(self):
        # IEEE802.11i passphrase vector; RFC8018 PBKDF2 construction.
        vectors=[(b'password',b'IEEE','f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e'),
                 (b'ThisIsAPassword',b'ThisIsASSID','0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af')]
        rng=random.Random(6630)
        cases=[(p,s,bytes.fromhex(v)) for p,s,v in vectors]
        for n in (8,9,16,31,63):
            p=rng.randbytes(n);s=rng.randbytes(min(n,32))
            cases.append((p,s,hashlib.pbkdf2_hmac('sha1',p,s,4096,32)))
        cases.append((b'pass\0word',b'SS\0ID',hashlib.pbkdf2_hmac('sha1',b'pass\0word',b'SS\0ID',4096,32)))
        for p,s,expected in cases:
            with self.subTest(pass_bytes=len(p),ssid_bytes=len(s)):
                out=self.output(34)
                self.assertEqual(self.api.pmk(p,len(p),s,len(s),C.byref(out,1)),0)
                self.assertEqual(out.raw[1:33],expected)
                self.assertEqual(out.raw[0::33],b'\xcc\xcc')

    def test_hmac_rfc2202_and_boundaries(self):
        vectors=[(b'\x0b'*20,b'Hi There','b617318655057264e28bc0b6fb378c8ef146be00'),
                 (b'Jefe',b'what do ya want for nothing?','effcdf6ae5eb2fa2d27416d5f184df9c259a7c79'),
                 (b'\xaa'*80,b'Test Using Larger Than Block-Size Key - Hash Key First','aa4ae5e15272d00e95705637ce8a3b55ed402112')]
        for k,d,expected in vectors:
            out=self.output(20)
            self.assertEqual(self.api.mac(k,len(k),d,len(d),out),0)
            self.assertEqual(out.raw.hex(),expected)
        rng=random.Random(3394)
        for kn in (0,1,16,20,63,64,65,80,2048):
            for n in (0,1,55,56,63,64,65,127,2048):
                k=rng.randbytes(kn);d=rng.randbytes(n);out=self.output(20)
                self.assertEqual(self.api.mac(k,kn,d,n,out),0)
                self.assertEqual(out.raw,hmac.digest(k,d,'sha1'))

    def test_ptk_prf512_order_and_alias(self):
        rng=random.Random(2202)
        for _ in range(20):
            pmk=rng.randbytes(32);ap=rng.randbytes(6);sta=rng.randbytes(6)
            an=rng.randbytes(32);sn=rng.randbytes(32)
            data=b''.join(sorted([ap,sta]))+b''.join(sorted([an,sn]))
            expected=b''.join(hmac.digest(pmk,b'Pairwise key expansion\0'+data+bytes([i]),'sha1') for i in range(4))[:64]
            for a,s,n1,n2 in ((ap,sta,an,sn),(sta,ap,sn,an)):
                out=self.output(66)
                self.assertEqual(self.api.ptk(pmk,a,s,n1,n2,C.byref(out,1)),0)
                self.assertEqual(out.raw[1:65],expected)
                self.assertEqual(out.raw[0::65],b'\xcc\xcc')
            alias=C.create_string_buffer(pmk+b'\0'*32,64)
            self.assertEqual(self.api.ptk(alias,ap,sta,an,sn,alias),0)
            self.assertEqual(alias.raw,expected)

    @staticmethod
    def packet(n):
        p=bytearray((i*17+3)&255 for i in range(n))
        p[0]=2;p[1]=3;p[2:4]=(n-4).to_bytes(2,'big');p[4]=2;p[5:7]=(0x13ca).to_bytes(2,'big')
        p[97:99]=(n-99).to_bytes(2,'big')
        return p

    def test_mic_zeroing_verify_and_corruption(self):
        k=bytes(range(16))
        for n in (99,100,127,128,256,2048):
            p=self.packet(n);zero=p.copy();zero[81:97]=b'\0'*16
            expected=hmac.digest(k,zero,'sha1')[:16]
            out=self.output(16)
            self.assertEqual(self.api.mic(k,bytes(p),n,out),0)
            self.assertEqual(out.raw,expected)
            p[81:97]=expected
            self.assertEqual(self.api.verify(k,bytes(p),n),0)
            for i in (17,80,81,96,n-1):
                q=p.copy();q[i]^=1
                self.assertNotEqual(self.api.verify(k,bytes(q),n),0)
        # Every MIC position must matter in the fixed-length comparison.
        for i in range(16):
            a=b'\0'*16;b=bytearray(a);b[i]=1
            self.assertEqual(self.api.equal(a,bytes(b),16),0)
        self.assertEqual(self.api.equal(k,k,16),1)

    def wrapped(self,k,p):
        openssl=shutil.which('openssl',path='/usr/bin:/bin') or shutil.which('openssl')
        self.assertIsNotNone(openssl, 'OpenSSL CLI with RFC3394 wrap support is required')
        return subprocess.run([openssl,'enc','-id-aes128-wrap','-K',k.hex(),'-iv','a6'*8],
                              input=p,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout

    def unwrap(self,k,c,capacity=None):
        out=self.output(2050);n=C.c_uint(123)
        ret=self.api.unwrap(k,c,len(c),C.byref(out,1),len(c)-8 if capacity is None else capacity,C.byref(n))
        return ret,n.value,out.raw

    def test_rfc3394_known_and_openssl_interop(self):
        # RFC3394 section4.1,128-bit KEK /128-bit plaintext.
        k=bytes.fromhex('000102030405060708090a0b0c0d0e0f')
        p=bytes.fromhex('00112233445566778899aabbccddeeff')
        c=bytes.fromhex('1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5')
        self.assertEqual(self.wrapped(k,p),c)
        ret,n,out=self.unwrap(k,c)
        self.assertEqual((ret,n,out[1:n+1]),(0,16,p))
        rng=random.Random(8018)
        for size in (16,24,32,40,128,512,2040):
            k=rng.randbytes(16);p=rng.randbytes(size);c=self.wrapped(k,p)
            ret,n,out=self.unwrap(k,c)
            self.assertEqual((ret,n,out[1:n+1]),(0,size,p))
            self.assertEqual(out[0],0xcc);self.assertEqual(out[n+1:],b'\xcc'*(2049-n))
            alias=C.create_string_buffer(c,len(c));n=C.c_uint()
            self.assertEqual(self.api.unwrap(k,alias,len(c),alias,len(c),C.byref(n)),0)
            self.assertEqual(alias.raw[:size],p)
            # Bad KEK and flipped IV/ciphertext must withhold plaintext.
            for badk,badc in ((bytes(x^1 for x in k),c),(k,bytes([c[0]^1])+c[1:]),(k,c[:-1]+bytes([c[-1]^1]))):
                ret,n,out=self.unwrap(badk,badc)
                self.assertEqual((ret,n),(2,0));self.assertEqual(out,b'\xcc'*2050)

    def test_argument_rejection_leaves_outputs(self):
        out=self.output(64)
        for pn,sn in ((7,1),(64,1),(8,0),(8,33),(0xffffffff,1)):
            self.assertEqual(self.api.pmk(b'p'*64,pn,b's'*33,sn,out),1)
            self.assertEqual(out.raw,b'\xcc'*64)
        self.assertEqual(self.api.ptk(None,b'a'*6,b'b'*6,b'n'*32,b'm'*32,out),1)
        self.assertEqual(self.api.mac(None,1,b'',0,out),1)
        self.assertEqual(self.api.mac(b'k',1,b'',2049,out),1)
        for n in (0,8,16,23,25,2049):
            ret,length,data=self.unwrap(b'k'*16,b'x'*n,2048)
            self.assertEqual((ret,length),(1,0));self.assertEqual(data,b'\xcc'*2050)
        ret,length,data=self.unwrap(b'k'*16,b'x'*24,15)
        self.assertEqual((ret,length),(1,0));self.assertEqual(data,b'\xcc'*2050)
        p=self.packet(99)
        for at in (0,1,2,4,6,97):
            q=p.copy();q[at]^=0xff
            self.assertEqual(self.api.mic(b'k'*16,bytes(q),99,out),1)
            self.assertEqual(out.raw,b'\xcc'*64)

    def test_sanitizers(self):
        src=self.path/'sanitized.c';src.write_text(WRAPPER+SANITIZER_MAIN)
        exe=self.path/'sanitized'
        subprocess.run(['clang','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        *self.flags,str(src),*self.inputs,'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)

    def test_arm64_c_cpp_and_real_provider_link(self):
        # Freestanding objects, not an SDK/kernel build. Declarations only for
        # libc memory functions; actual crypto providers compiled unchanged.
        shim=self.path/'freestanding';shim.mkdir()
        (shim/'string.h').write_text('#include <stddef.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\nvoid *memcpy(void *,const void *,size_t);\nvoid *memset(void *,int,size_t);\nvoid *memmove(void *,const void *,size_t);\nint memcmp(const void *,const void *,size_t);\n#ifdef __cplusplus\n}\n#endif\n')
        flags=['--target=aarch64-none-elf','-ffreestanding','-O2','-Wall','-Wextra','-Werror',
               '-I'+str(shim),*self.flags]
        objects=[]
        for i,src in enumerate([str(self.src),*self.inputs]):
            obj=self.path/f'arm{i}.o';objects.append(str(obj))
            subprocess.run(['clang',*flags,'-c',src,'-o',str(obj)],check=True)
        linked=self.path/'crypto-arm64.o'
        subprocess.run(['ld.lld','-r',*objects,'-o',str(linked)],check=True)
        undefined=subprocess.check_output(['nm','-u',str(linked)],text=True)
        self.assertEqual({line.split()[-1] for line in undefined.splitlines()}, {'memcpy','memset'})
        subprocess.run(['clang++',*flags,'-std=c++14','-x','c++','-c',str(self.src),'-o',str(self.path/'cpp.o')],check=True)
        # Mach-O arm64 iOS codegen of the actual adapter as used by SDIO C++.
        subprocess.run(['clang++',*flags[1:],'--target=arm64-apple-ios16.7','-std=c++14','-x','c++',
                        '-c',str(self.src),'-o',str(self.path/'ios.o')],check=True)

if __name__=='__main__':
    unittest.main()
