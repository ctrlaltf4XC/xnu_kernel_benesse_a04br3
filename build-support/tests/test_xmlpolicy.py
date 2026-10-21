# SPDX-License-Identifier: MIT
import ctypes, pathlib, plistlib, subprocess, tempfile, unittest

class XMLPolicy(unittest.TestCase):
 def test_preservation_and_rejection(self):
  inc=pathlib.Path(__file__).resolve().parents[2]/'bsd/dev'
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'test.c').write_text('#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include <errno.h>\n#include "a04br3_bplist.h"\nint edit(const unsigned char*s,size_t n,unsigned char*o,size_t c,size_t*w){return a04_bp_disable_fp(s,n,o,c,w);}\n')
   subprocess.run(['cc','-shared','-fPIC','-fsanitize=undefined','-I',str(inc),str(p/'test.c'),'-o',str(p/'test.so')],check=True)
   f=ctypes.CDLL(str(p/'test.so')).edit
   def edit(b,cap=32768):
    out=ctypes.create_string_buffer(32768);n=ctypes.c_size_t();e=f(b,len(b),out,cap,ctypes.byref(n));return e,out.raw[:n.value]
   for source in [{},{'com.apple.other':False},{'com.apple.fairplayd.H2':False,'com.apple.other':True},{'com.apple.fairplayd.H2':True}]:
    b=plistlib.dumps(source);e,o=edit(b);self.assertEqual(e,0);expected=dict(source);expected['com.apple.fairplayd.H2']=True;self.assertEqual(plistlib.loads(o),expected);self.assertEqual(edit(o),(0,o))
    if source.get('com.apple.fairplayd.H2'):self.assertEqual(o,b)
    elif 'com.apple.fairplayd.H2' in source:self.assertEqual(o,b.replace(b'<false/>',b'<true/>'))
    else:self.assertIn(b'<key>com.apple.fairplayd.H2</key><true/>',o)
    self.assertNotEqual(edit(b,1)[0],0)
   b=plistlib.dumps({'com.apple.other':False})
   for i in range(len(b.rstrip())):
    e,o=edit(b[:i]);self.assertNotEqual(e,0)
   for bad in [plistlib.dumps({'nested':{'k':True}}),plistlib.dumps({'str':'value'}),b'<?xml?><plist><dict><key>com.apple.fairplayd.H2</key><true/><key>com.apple.fairplayd.H2</key><false/></dict></plist>',b'<plist><dict><key>&unknown;</key><true/></dict></plist>',b'<!DOCTYPE plist [<!ENTITY x "x">]><plist><dict/></plist>']:
    self.assertNotEqual(edit(bad)[0],0)
