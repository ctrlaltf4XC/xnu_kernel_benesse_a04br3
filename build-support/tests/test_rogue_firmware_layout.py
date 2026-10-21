# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Host tests for the allocation-free kernel firmware parser.

The real vendor blob is read externally (A04_RGX_FIRMWARE overrides the board
path); it is never a fixture committed to this repository. Generated malformed
images, binaries and sanitizer harnesses live only in TemporaryDirectory.
"""
import ctypes as C
import importlib.util
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import unittest

KERNEL = Path(__file__).resolve().parents[2]
HEADER = KERNEL / 'iokit/Kernel/arm/rogue'
ROOT = KERNEL.parent.parent.parent


class Section(C.Structure):
    _fields_ = [(k, C.c_uint32) for k in
                ('id', 'kind', 'base', 'maximum', 'allocated', 'offset')]


class Segment(C.Structure):
    _fields_ = [(k, C.c_uint32) for k in
                ('kind', 'target', 'source', 'address', 'filesz', 'memsz',
                 'permissions', 'alignment')]


class Layout(C.Structure):
    _fields_ = [(k, C.c_uint32) for k in
                ('entry_point', 'code_bytes', 'data_bytes', 'segment_count')]
    _fields_ += [('sections', Section * 6), ('segments', Segment * 64)]


def fixture():
    blob = bytearray(8192)
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    struct.pack_into('<16sHHIIIIIHHHHHH', blob, 0, ident,
                     2, 8, 1, 0x1001, 52, 0, 0, 52, 32, 3, 0, 0, 0)
    for i, (source, address, filesz, memsz, flags) in enumerate(
            ((256, 0x1000, 4, 8, 5), (260, 0x4004, 3, 7, 6),
             (263, 0x2000, 2, 4, 5))):
        struct.pack_into('<8I', blob, 52+i*32,
                         1, source, address, 0, filesz, memsz, flags, 1)
    blob[256:265] = b'TESTabcXY'
    struct.pack_into('<4IQI', blob, 4096,
                     1, 32, 6, 24, 0x001600280036001e, 0)
    for n in range(6):
        struct.pack_into('<6I', blob, 4128+n*24,
                         4+n, 1 if n < 3 else 2, 0x1000+n*0x1000,
                         16, 16, (n % 3)*32)
    return blob


def changed(blob, offset, value, fmt='<I'):
    result = bytearray(blob)
    struct.pack_into(fmt, result, offset, value)
    return result


class FirmwareLayout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.work = Path(cls.directory.name)
        cls.cc = shutil.which('clang') or shutil.which('cc')
        cls.cxx = shutil.which('clang++') or shutil.which('c++')
        wrapper = cls.work / 'wrapper.c'
        wrapper.write_text('''#include "A04BR3RogueFirmware.h"
#ifdef __cplusplus
extern "C" {
#endif
int parse(const void *p,size_t n,struct a04_rgx_fw_layout *out) {
 return a04_rgx_fw_parse(p,n,out);
}
int unpack(const void *p,size_t n,void *c,size_t cn,void *d,size_t dn,
 struct a04_rgx_fw_layout *out) {
 return a04_rgx_fw_unpack(p,n,c,cn,d,dn,out);
}
#ifdef __cplusplus
}
#endif
''')
        common = ['-Wall', '-Wextra', '-Werror', '-pedantic', '-I', str(HEADER)]
        subprocess.run([cls.cc, '-std=c11', *common, '-shared', '-fPIC',
                        str(wrapper), '-o', str(cls.work / 'parser.so')], check=True)
        # The same header also builds in kernel-style C++ with no runtime,
        # exceptions, RTTI or standard-library dependency.
        subprocess.run([cls.cxx, '-x', 'c++', '-std=c++14', '-ffreestanding',
                        '-fno-exceptions', '-fno-rtti', *common, '-c',
                        str(wrapper), '-o', str(cls.work / 'parser-cxx.o')], check=True)
        cls.lib = C.CDLL(str(cls.work / 'parser.so'))
        cls.lib.parse.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(Layout)]
        cls.lib.parse.restype = C.c_int
        cls.lib.unpack.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t,
                                   C.c_void_p, C.c_size_t, C.POINTER(Layout)]
        cls.lib.unpack.restype = C.c_int
        spec = importlib.util.spec_from_file_location(
            'rogue_firmware_reference', KERNEL.parent / 'device/tools/rogue_firmware.py')
        cls.reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.reference)

    def parse(self, blob):
        source = C.create_string_buffer(bytes(blob))
        layout = Layout()
        C.memset(C.byref(layout), 0xa5, C.sizeof(layout))
        before = bytes(layout)
        result = self.lib.parse(source, len(blob), C.byref(layout))
        if result:
            self.assertEqual(bytes(layout), before, 'failed parse changed caller layout')
        return result, layout

    def compare_reference(self, blob):
        manifest, buffers = self.reference.stage(blob)
        result, layout = self.parse(blob)
        self.assertEqual(result, 0)
        self.assertEqual(layout.entry_point, manifest['entry_point'])
        self.assertEqual((layout.code_bytes, layout.data_bytes),
                         (len(buffers[1]), len(buffers[2])))
        self.assertEqual(layout.segment_count, len(manifest['segments']))
        for section in manifest['sections']:
            actual = layout.sections[section['id']-4]
            self.assertEqual({k: getattr(actual, k) for k, _ in Section._fields_}, section)
        for i, segment in enumerate(manifest['segments']):
            for key, expected in segment.items():
                self.assertEqual(getattr(layout.segments[i], key), expected)
        self.assertEqual(bytes(layout.segments)[layout.segment_count*C.sizeof(Segment):],
                         bytes((64-layout.segment_count)*C.sizeof(Segment)))
        source = C.create_string_buffer(bytes(blob))
        code = C.create_string_buffer(b'\xa5' * (layout.code_bytes+17))
        data = C.create_string_buffer(b'\xa5' * (layout.data_bytes+17))
        output = Layout()
        self.assertEqual(self.lib.unpack(source, len(blob), code, len(code),
                                         data, len(data), C.byref(output)), 0)
        self.assertEqual(code.raw[:layout.code_bytes], buffers[1])
        self.assertEqual(data.raw[:layout.data_bytes], buffers[2])
        self.assertEqual(code.raw[layout.code_bytes:], b'\xa5'*17+b'\0')
        self.assertEqual(data.raw[layout.data_bytes:], b'\xa5'*17+b'\0')
        self.assertEqual(bytes(output), bytes(layout))

    def test_valid_unaligned_reordered_sections_and_reference_bytes(self):
        blob = fixture()
        self.compare_reference(blob)
        reordered = bytearray(blob)
        for i in range(6):
            reordered[4128+i*24:4152+i*24] = blob[4128+(5-i)*24:4152+(5-i)*24]
        self.compare_reference(reordered)
        source = C.create_string_buffer(b'_' + bytes(blob))
        layout = Layout()
        self.assertEqual(self.lib.parse(C.byref(source, 1), len(blob), C.byref(layout)), 0)
        # Non-PT_LOAD headers are ignored even if their fields are meaningless.
        extended = changed(blob, 44, 4, '<H')
        struct.pack_into('<8I', extended, 148, 0, *([0xffffffff]*7))
        self.compare_reference(extended)

    def test_null_size_and_elf_header_bounds(self):
        layout = Layout()
        self.assertEqual(self.lib.parse(None, 8192, C.byref(layout)), 1)
        self.assertEqual(self.lib.parse(C.c_void_p(1), 8192, None), 1)
        # Sizes are rejected before the pointer is read.
        for n in (0, 1, 4096, 32*1024*1024+1, C.c_size_t(-1).value):
            self.assertEqual(self.lib.parse(C.c_void_p(1), n, C.byref(layout)), 2)
        blob = fixture()
        for offset, value, fmt in ((0, 0, '<I'), (4, 2, '<B'), (5, 2, '<B'),
                                   (16, 1, '<H'), (18, 40, '<H'), (20, 2, '<I'),
                                   (40, 51, '<H'), (42, 31, '<H'), (44, 0, '<H'),
                                   (44, 65, '<H'), (28, 51, '<I'),
                                   (28, 0xffffffff, '<I'), (28, 4001, '<I')):
            with self.subTest(offset=offset, value=value):
                self.assertNotEqual(self.parse(changed(blob, offset, value, fmt))[0], 0)
        # Size 4097..4147 has an information trailer, but no complete ELF header.
        for size in range(4097, 4148):
            tiny = bytearray(size)
            tiny[size-4096:] = blob[4096:]
            self.assertNotEqual(self.parse(tiny)[0], 0)

    def test_firmware_info_and_section_validation(self):
        blob = fixture()
        bad = ((4096, 2), (4100, 0xffffffff), (4104, 0), (4104, 11),
               (4104, 5), (4108, 0xffffffff), (4112, 0), (4120, 1),
               (4128, 3), (4128, 10), (4152, 4), (4132, 2),
               (4144, 0), (4144, 17), (4140, 0),
               (4136, 0xfffffff8), (4148, 0xfffffff0),
               (4148, 32*1024*1024-15), (4160, 0x1008), (4172, 8),
               # Cross-kind virtual overlap is forbidden too.
               (4128+3*24+8, 0x1000))
        for offset, value in bad:
            with self.subTest(offset=offset, value=value):
                self.assertNotEqual(self.parse(changed(blob, offset, value))[0], 0)
        # Exactly ending at 2^32 is legal; the arithmetic must not wrap.
        self.assertEqual(self.parse(changed(blob, 4128+2*24+8, 0xfffffff0))[0], 0)
        # A sparse destination may end at the limit, but not one byte beyond.
        edge = changed(blob, 4128+2*24+20, 32*1024*1024-16)
        self.assertEqual(self.parse(edge)[1].code_bytes, 32*1024*1024)

    def test_segment_source_destination_overlap_and_entry(self):
        blob = fixture()
        for offset, value in ((52+4, 4093), (52+4, 0xffffffff),
                               (52+16, 9), (52+20, 17), (52+8, 0x9000),
                               (52+8, 0xfffffffc), (52+20, 0xffffffff),
                               (24, 0x3001), (52+24, 4),
                               (52+2*32+8, 0x1004)):
            with self.subTest(offset=offset, value=value):
                self.assertNotEqual(self.parse(changed(blob, offset, value))[0], 0)
        # Exclusive end of source range may touch the info page, never enter it.
        self.assertEqual(self.parse(changed(blob, 52+4, 4092))[0], 0)
        self.assertEqual(self.parse(changed(blob, 24, 0x1000))[0], 0)
        self.assertNotEqual(self.parse(changed(blob, 24, 0x1008))[0], 0)

    def test_max_segments_exact_table_end_and_32bit_end(self):
        blob = fixture()
        # Every one of the 64 slots is a PT_LOAD, with adjacent (not
        # overlapping) one-byte memory ranges in an enlarged code section.
        many = changed(blob, 44, 64, '<H')
        struct.pack_into('<II', many, 4128+12, 128, 128)
        struct.pack_into('<I', many, 4128+24+20, 128)
        struct.pack_into('<I', many, 4128+48+20, 160)
        for i in range(64):
            struct.pack_into('<8I', many, 52+i*32,
                             1, 0, 0x1000+i, 0, 1, 1, 5, 1)
        self.compare_reference(many)
        self.assertEqual(self.parse(many)[1].segment_count, 64)
        edge = changed(blob, 28, 4000)
        edge[4000:4096] = blob[52:148]
        self.compare_reference(edge)
        # Both section and segment may end exactly at 2**32, never wrap.
        high = changed(blob, 4128+2*24+8, 0xfffffff0)
        high = changed(high, 52+2*32+8, 0xfffffffc)
        high = changed(high, 24, 0xfffffffd)
        self.compare_reference(high)
        bad = changed(high, 52+2*32+20, 5)
        self.assertNotEqual(self.parse(bad)[0], 0)

    def test_unpack_failure_is_atomic_capacity_and_alias(self):
        blob = fixture()
        source = C.create_string_buffer(bytes(blob))
        code = C.create_string_buffer(b'C'*128)
        data = C.create_string_buffer(b'D'*128)
        out = Layout()
        C.memset(C.byref(out), 0xa5, C.sizeof(out))
        original = (source.raw, code.raw, data.raw, bytes(out))
        for cp, cn, dp, dn, op in (
                (None, 128, data, 128, C.byref(out)),
                (code, 79, data, 128, C.byref(out)),
                (code, 128, data, 79, C.byref(out)),
                (code, 128, code, 128, C.byref(out)),
                (source, 8192, data, 128, C.byref(out)),
                (code, 128, source, 8192, C.byref(out)),
                (code, 128, data, 128, C.cast(source, C.POINTER(Layout))),
                (C.byref(out), C.sizeof(out), data, 128, C.byref(out)),
                (code, 128, C.byref(out), C.sizeof(out), C.byref(out))):
            self.assertNotEqual(self.lib.unpack(source, len(blob), cp, cn, dp, dn, op), 0)
            self.assertEqual((source.raw, code.raw, data.raw, bytes(out)), original)
        # A late invalid segment must not leave earlier valid copies behind.
        malformed = C.create_string_buffer(bytes(changed(blob, 52+2*32+20, 17)))
        self.assertNotEqual(self.lib.unpack(malformed, len(blob), code, 128,
                                            data, 128, C.byref(out)), 0)
        self.assertEqual((source.raw, code.raw, data.raw, bytes(out)), original)
        # Reject uintptr_t wrap before any output dereference.
        overflow = C.c_void_p(C.c_size_t(-1).value-16)
        self.assertEqual(self.lib.unpack(source, len(blob), overflow, 128,
                                         data, 128, C.byref(out)), 1)
        self.assertEqual((source.raw, code.raw, data.raw, bytes(out)), original)
        self.assertEqual(self.lib.unpack(source, len(blob), code, 128, data, 128, None), 0)
        self.assertEqual(self.lib.parse(source, len(blob), C.cast(source, C.POINTER(Layout))), 10)

    def test_deterministic_mutations_against_python_reference(self):
        rng = random.Random(0xa04)
        base = fixture()
        for _ in range(3000):
            blob = bytearray(base)
            for _ in range(rng.randrange(1, 5)):
                offset = rng.choice((rng.randrange(0, 148), rng.randrange(4096, 4272)))
                blob[offset] ^= 1 << rng.randrange(8)
            try:
                manifest, _ = self.reference.stage(blob)
                expected = True
            except ValueError:
                expected = False
            result, layout = self.parse(blob)
            self.assertEqual(result == 0, expected)
            if expected:
                self.assertEqual(layout.entry_point, manifest['entry_point'])

    def test_arm64_freestanding_object(self):
        clang = shutil.which('clang++')
        if not clang:
            self.skipTest('clang++ required for ARM64 cross compilation')
        subprocess.run([clang, '--target=arm64-apple-darwin', '-x', 'c++',
                        '-std=c++14', '-ffreestanding', '-fno-exceptions',
                        '-fno-rtti', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-Wframe-larger-than=4096', '-I', str(HEADER), '-c',
                        str(self.work / 'wrapper.c'),
                        '-o', str(self.work / 'parser-arm64.o')], check=True)

    def test_external_vendor_image_matches_python_reference(self):
        path = Path(os.environ.get('A04_RGX_FIRMWARE', str(
            ROOT / 'port/gsi/vendor/benesse/a04br3/proprietary/vendor/firmware/rgx.fw.22.40.54.30')))
        if not path.is_file():
            if 'A04_RGX_FIRMWARE' in os.environ:
                self.fail('explicit external firmware file is missing')
            self.skipTest('external board firmware not present')
        blob = path.read_bytes()
        self.compare_reference(blob)
        layout = self.parse(blob)[1]
        self.assertEqual((layout.code_bytes, layout.data_bytes), (102400, 20480))

    def test_sanitized_guard_pages_truncation_and_mutations(self):
        fixture_path = self.work / 'synthetic.bin'
        fixture_path.write_bytes(fixture())
        harness = self.work / 'guard.c'
        harness.write_text(r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "A04BR3RogueFirmware.h"
int main(int argc,char **argv) {
 assert(argc==2);
 FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char seed[8192];assert(fread(seed,1,sizeof(seed),f)==sizeof(seed));fclose(f);
 size_t page=(size_t)sysconf(_SC_PAGESIZE);
 size_t capacity=((sizeof(seed)+page-1)/page)*page;
 unsigned char *map=mmap(NULL,capacity+page,PROT_READ|PROT_WRITE,
                        MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 assert(map!=MAP_FAILED);assert(!mprotect(map+capacity,page,PROT_NONE));
 struct a04_rgx_fw_layout out,saved;
 for(size_t n=0;n<=sizeof(seed);n++) {
  unsigned char *input=map+capacity-n;memcpy(input,seed,n);
  memset(&out,0xa5,sizeof(out));saved=out;
  int error=a04_rgx_fw_parse(input,n,&out);
  if(n!=sizeof(seed)){assert(error);assert(!memcmp(&out,&saved,sizeof(out)));}
  else assert(!error);
 }
 /* Exercise every prefix length with a valid info table. Missing ELF fields
  * can otherwise be hidden by a rejected/shifted trailer. */
 for(size_t prefix=1;prefix<160;prefix++) {
  size_t n=4096+prefix;unsigned char *input=map+capacity-n;
  memcpy(input,seed,prefix);memcpy(input+prefix,seed+4096,4096);
  memset(&out,0xa5,sizeof(out));saved=out;
  int error=a04_rgx_fw_parse(input,n,&out);
  assert(error);assert(!memcmp(&out,&saved,sizeof(out)));
 }
 unsigned state=0xa04;
 for(unsigned i=0;i<20000;i++) {
  unsigned char *input=map+capacity-sizeof(seed);memcpy(input,seed,sizeof(seed));
  for(unsigned j=0;j<4;j++) {
   state=state*1664525U+1013904223U;
   unsigned pos=(state>>16)%2 ? (state%148) : (4096+state%176);
   input[pos]^=(unsigned char)(state>>24);
  }
  unsigned char code[128],data[128],oldcode[128],olddata[128];
  memset(code,0xa5,sizeof(code));memset(data,0x5a,sizeof(data));
  memcpy(oldcode,code,sizeof(code));memcpy(olddata,data,sizeof(data));
  memset(&out,0xa5,sizeof(out));saved=out;
  int error=a04_rgx_fw_unpack(input,sizeof(seed),code,sizeof(code),data,sizeof(data),&out);
  if(error) {
   assert(!memcmp(&out,&saved,sizeof(out)));
   assert(!memcmp(code,oldcode,sizeof(code)));assert(!memcmp(data,olddata,sizeof(data)));
  }
 }
 assert(!munmap(map,capacity+page));
 puts("PASS: guarded prefixes and 20000 mutations; atomic output on failure");
}
''')
        executable = self.work / 'guard'
        subprocess.run([self.cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I', str(HEADER), str(harness), '-o', str(executable)], check=True)
        subprocess.run([str(executable), str(fixture_path)], check=True)


if __name__ == '__main__':
    unittest.main()
