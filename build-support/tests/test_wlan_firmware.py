# SPDX-License-Identifier: MIT
"""External MTKW inspection and CPU-only sender verification. No embedded FW.
CLI: python3 test_wlan_firmware.py --inspect /explicit/WIFI_RAM_CODE_MT6630
The output is metadata only; the input is never copied into the source tree.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import unittest
import zlib
import test_wmt_protocol as codecs
from test_wlan_host import FAKE

ROOT = Path(__file__).resolve().parents[5]
REFERENCE = ROOT / 'port/edk2/reference/a04br3-stock-firmware/WIFI_RAM_CODE_MT6630'


def inspect(path):
    with Path(path).open('rb') as stream:
        b = stream.read(16 * 1024 * 1024 + 1)
    if not 32 <= len(b) <= 16*1024*1024:
        raise ValueError('length')
    sig, crc, count, reserved = struct.unpack_from('<4I', b)
    if sig != 0x574b544d or reserved or not 1 <= count <= 16 or len(b) < 16+16*count:
        raise ValueError('header')
    if zlib.crc32(b[8:]) != crc:
        raise ValueError('CRC')
    regions = []
    for i in range(count):
        off, reserved, n, addr = struct.unpack_from('<4I', b, 16+16*i)
        if reserved or off < 16+16*count or off > len(b) or not n or n > len(b)-off or addr+n > 2**32:
            raise ValueError('region')
        for r in regions:
            if off < r['offset']+r['length'] and r['offset'] < off+n or addr < r['address']+r['length'] and r['address'] < addr+n:
                raise ValueError('overlap')
        regions.append(dict(offset=off, length=n, address=addr, pda_packets=(n+2047)//2048))
    return dict(source=str(Path(path).resolve()), bytes=len(b), sha256=hashlib.sha256(b).hexdigest(),
                crc32=f'{crc:08x}', sections=regions,
                trailing_bytes=len(b)-max(r['offset']+r['length'] for r in regions),
                operation='CPU inspection only; firmware execution unverified')


class WLANExternalFirmware(unittest.TestCase):
    run_c = codecs.WMTProtocol.run_c

    def check_blob(self, path, expected, candidate):
        if not path.is_file():
            self.skipTest('external board firmware not present; supply explicit asset')
        info = inspect(path)
        self.assertEqual(info['bytes'], 237008)
        self.assertEqual(info['sha256'], expected)
        self.assertEqual([r['pda_packets'] for r in info['sections']], [107, 10])
        self.assertEqual(info['trailing_bytes'], 16)
        digest = ','.join('0x'+info['sha256'][i:i+2] for i in range(0,64,2))
        self.run_c(FAKE + '\n#include <stdio.h>\n' + r'''
int main(void){
 static uint8_t b[237008];FILE *file=fopen(PATH,"rb");assert(file);
 assert(fread(b,1,sizeof(b),file)==sizeof(b) && fgetc(file)==EOF);fclose(file);
 struct a04_wlan_firmware fw;struct a04_wmt_identity id={0x6630,0x8a12,0x8a11};
 uint8_t digest[32]={DIGEST};
 assert(!a04_wlan_firmware_select(&id,b,sizeof(b),digest,&fw));
 assert(fw.candidate==CANDIDATE && fw.count==2 && fw.section[0].address==0x91400 && fw.section[1].address==0x209c000);
 digest[0]^=1;assert(a04_wlan_firmware_select(&id,b,sizeof(b),digest,&fw)==A04_WLAN_CRC && !fw.count);digest[0]^=1;
 id.rom=0x8a10;assert(a04_wlan_firmware_select(&id,b,sizeof(b),digest,&fw)==A04_WLAN_UNSUPPORTED);id.rom=0x8a11;
 assert(!a04_wlan_firmware_select(&id,b,sizeof(b),digest,&fw));
 struct card c;struct a04_sdio_io io;struct a04_sdio_transport t;struct a04_wlan_host h;
 setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));c.fw=&fw;
 struct a04_wlan_download_state s;memset(&s,0,sizeof(s));
 assert(!a04_wlan_download(&h,&fw,&s));
 assert(s.ready && s.packets==117 && s.configured==2 && c.txs==120 && c.start);
 for(unsigned fault=1;fault<=120;fault++){
  setup(&c,&io,&t);assert(!a04_wlan_host_open(&h,&t));c.fw=&fw;c.fail_tx=fault;
  memset(&s,0,sizeof(s));assert(a04_wlan_download(&h,&fw,&s)==24);
  assert(t.failed && h.failed && s.failed && c.txs==fault && !s.ready);
  assert(a04_wlan_download(&h,&fw,&s)==A04_WLAN_STATE && c.txs==fault);
 }
 return 0;
}
'''.replace('PATH', json.dumps(str(path))).replace('DIGEST', digest).replace('CANDIDATE', str(candidate)))

    def test_reference_blob(self):
        self.check_blob(REFERENCE, 'f28e4c9f03821c1ec6729b3b9b9c05bf82575b5ed58cce447e653506f6bddd45', 1)

    def test_explicit_vendor_blob(self):
        path = ROOT/'port/gsi/vendor/benesse/a04br3/proprietary/vendor/firmware/WIFI_RAM_CODE_MT6630'
        self.check_blob(path, '07fe1a225e6b9de6b310894db3b40bcc1cb66107b913e4c43eb6a62089aada22', 2)

if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--inspect':
        print(json.dumps(inspect(sys.argv[2]), indent=2))
    else:
        unittest.main()
