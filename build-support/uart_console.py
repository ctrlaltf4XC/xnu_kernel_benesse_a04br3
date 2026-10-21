#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Build copied, immutable device userland source; embed generated output only.

The userland source lives in the device repository. Its pin predates this
kernel revision; the later device revision pins this kernel (no cycle).
"""
import hashlib,json,subprocess,urllib.request
from pathlib import Path
SUPPORT=Path(__file__).resolve().parent

def stage_console(work_source,cache):
    pin=json.loads((SUPPORT/'uart-console-source.json').read_text())
    cache=Path(cache)/('uart-console-'+pin['commit']);src=cache/'tools';src.mkdir(parents=True,exist_ok=True)
    for name,digest in pin['files'].items():
        path=src/Path(name).name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            url=f"https://raw.githubusercontent.com/{pin['github_repository']}/{pin['commit']}/{name}"
            with urllib.request.urlopen(url,timeout=60) as response:data=response.read(2*1024*1024)
            if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('console source mismatch: '+name)
            path.write_bytes(data)
    binary=cache/'a04console';meta=cache/'build.json'
    previous=json.loads(meta.read_text()) if meta.is_file() else {}
    if not binary.is_file() or previous.get('pin')!=pin or previous.get('binary_sha256')!=hashlib.sha256(binary.read_bytes()).hexdigest():
        subprocess.run(['bash',str(src/'build-uart-console.sh'),str(binary)],check=True)
        data=binary.read_bytes()
        if not 32<=len(data)<=2*1024*1024 or data[:4]!=b'\xcf\xfa\xed\xfe':raise ValueError('invalid console Mach-O')
        previous={'pin':pin,'binary_sha256':hashlib.sha256(data).hexdigest(),'binary_bytes':len(data)}
        meta.write_text(json.dumps(previous,indent=2)+'\n')
    data=binary.read_bytes()
    text='/* Generated from pinned source by build-support/uart_console.py. */\nstatic const unsigned char a04br3_console_payload[] = {\n'
    text+='\n'.join(','.join('0x%02x'%x for x in data[i:i+24])+',' for i in range(0,len(data),24))+'\n};\n'
    target=Path(work_source)/'bsd/dev/a04br3_console_payload.inc'
    if not target.is_file() or target.read_text()!=text:target.write_text(text)
    return previous
