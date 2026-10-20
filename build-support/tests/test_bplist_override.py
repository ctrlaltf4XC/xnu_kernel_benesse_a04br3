# SPDX-License-Identifier: MIT
from pathlib import Path
import ctypes,datetime,plistlib,random,subprocess,tempfile,unittest

class BinaryPlistOverride(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();d=Path(cls.tmp.name)
  inc=Path(__file__).resolve().parents[2]/'bsd/dev'
  c=d/'bridge.c';c.write_text('#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include <errno.h>\n#include "a04br3_bplist.h"\nint merge(const unsigned char *s,size_t n,unsigned char *o,size_t cap,size_t *len){return a04_bp_disable_fp(s,n,o,cap,len);}\n')
  lib=d/'bridge.so';subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(inc),str(c),'-o',str(lib)],check=True)
  cls.lib=ctypes.CDLL(str(lib));cls.lib.merge.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def apply(self,data,cap=32768):
  src=ctypes.create_string_buffer(data);out=ctypes.create_string_buffer(32768);n=ctypes.c_size_t();rc=self.lib.merge(src,len(data),out,cap,ctypes.byref(n));return rc,out.raw[:n.value]
 def test_preserves_opaque_unrelated_values_and_unicode(self):
  for root in [{},{'other':True},{'com.apple.fairplayd.H2':False,'other':False},{'other':['one',{'nested':b'\x00\xff'}],'unicode-\u65e5':42,'date':datetime.datetime(2026,1,1),'uid':plistlib.UID(9)},{'service'+str(i):False for i in range(300)}]:
   source=plistlib.dumps(root,fmt=plistlib.FMT_BINARY);rc,got=self.apply(source);self.assertEqual(rc,0);expected=dict(root);expected['com.apple.fairplayd.H2']=True;self.assertEqual(plistlib.loads(got),expected)
   rc,again=self.apply(got);self.assertEqual(rc,0);self.assertEqual(again,got)
 def test_already_disabled_is_byte_identical(self):
  src=plistlib.dumps({'com.apple.fairplayd.H2':True,'other':b'data'},fmt=plistlib.FMT_BINARY);rc,out=self.apply(src);self.assertEqual((rc,out),(0,src))
 def test_rejects_unknown_target_types_and_small_output(self):
  src=plistlib.dumps({'com.apple.fairplayd.H2':'unknown'},fmt=plistlib.FMT_BINARY);self.assertNotEqual(self.apply(src)[0],0)
  self.assertNotEqual(self.apply(plistlib.dumps({},fmt=plistlib.FMT_XML))[0],0)
  self.assertNotEqual(self.apply(plistlib.dumps({},fmt=plistlib.FMT_BINARY),10)[0],0)
 def test_truncations_and_hostile_trailers(self):
  src=plistlib.dumps({'one':True},fmt=plistlib.FMT_BINARY)
  for i in range(len(src)):self.assertNotEqual(self.apply(src[:i])[0],0)
  for at in [-26,-25]:
   for value in [0,9,255]:
    b=bytearray(src);b[at]=value;self.assertNotEqual(self.apply(bytes(b))[0],0)
  rng=random.Random(8167)
  for _ in range(2000):
   b=bytearray(src);at=rng.randrange(len(b));b[at]=rng.randrange(256);rc,out=self.apply(bytes(b))
   if not rc:self.assertLessEqual(len(out),32768)
