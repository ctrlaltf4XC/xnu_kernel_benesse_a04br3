#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Build copied, immutable device userland source; embed generated output only.

The userland source lives in the device repository. Its pin predates this
kernel revision; the later device revision pins this kernel (no cycle).
Old console-only pins remain supported. Each optional helper needs a complete
pinned source set; an incomplete set fails rather than embedding a placeholder.
"""
import hashlib,json,subprocess,urllib.request
from pathlib import Path
SUPPORT=Path(__file__).resolve().parent
CAPTURE_FILES=frozenset(('tools/a04-fb-capture.c','tools/a04-fb-capture.h',
    'tools/a04-fb-capture-digest.h','tools/build-a04-fb-capture.sh'))
KEYBOARD_STATE_FILES=frozenset(('tools/a04br3_keyboard_state.c',
    'tools/a04br3_keyboard_state_sources.json','tools/build-a04br3-keyboard-state.sh'))
UI_TEST_FILES=frozenset(('tools/a04-ui-test.c','tools/build-a04-ui-test.sh'))

def _binary_bytes(path,label):
    data=path.read_bytes()
    if not 32<=len(data)<=2*1024*1024 or data[:4]!=b'\xcf\xfa\xed\xfe' or \
        int.from_bytes(data[4:8],'little')!=0x100000c or int.from_bytes(data[12:16],'little')!=2:
        raise ValueError('invalid '+label+' arm64 executable Mach-O')
    return data

def _build_program(src,cache,name,script,pin,previous,label):
    binary=cache/name
    if not binary.is_file() or previous.get('pin')!=pin or \
        previous.get('binary_sha256')!=hashlib.sha256(binary.read_bytes()).hexdigest():
        subprocess.run(['bash',str(src/script),str(binary)],check=True)
    data=_binary_bytes(binary,label)
    return data,{'pin':pin,'binary_sha256':hashlib.sha256(data).hexdigest(),'binary_bytes':len(data)}

def _array(name,data):
    return 'static const unsigned char '+name+'[] = {\n'+ \
        '\n'.join(','.join('0x%02x'%x for x in data[i:i+24])+',' for i in range(0,len(data),24))+'\n};\n'

def stage_console(work_source,cache):
    pin=json.loads((SUPPORT/'uart-console-source.json').read_text())
    capture=CAPTURE_FILES.intersection(pin['files'])
    if capture and capture!=CAPTURE_FILES:
        raise ValueError('incomplete pinned capture source set: '+','.join(sorted(CAPTURE_FILES-capture)))
    ui_test=UI_TEST_FILES.intersection(pin['files'])
    if ui_test and ui_test!=UI_TEST_FILES:
        raise ValueError('incomplete pinned UI test source set: '+','.join(sorted(UI_TEST_FILES-ui_test)))
    keyboard_state=KEYBOARD_STATE_FILES.intersection(pin['files'])
    if keyboard_state and keyboard_state!=KEYBOARD_STATE_FILES:
        raise ValueError('incomplete pinned keyboard state source set: '+','.join(sorted(KEYBOARD_STATE_FILES-keyboard_state)))
    cache=Path(cache)/('uart-console-'+pin['commit']);src=cache/'tools';src.mkdir(parents=True,exist_ok=True)
    for name,digest in pin['files'].items():
        path=src/Path(name).name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            url=f"https://raw.githubusercontent.com/{pin['github_repository']}/{pin['commit']}/{name}"
            with urllib.request.urlopen(url,timeout=60) as response:data=response.read(2*1024*1024)
            if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('console source mismatch: '+name)
            path.write_bytes(data)
    meta=cache/'build.json'
    previous=json.loads(meta.read_text()) if meta.is_file() else {}
    data,result=_build_program(src,cache,'a04console','build-uart-console.sh',pin,previous,'console')
    text='/* Generated from pinned source by build-support/uart_console.py. */\n'
    text+=_array('a04br3_console_payload',data)
    if capture:
        data,capture_result=_build_program(src,cache,'a04-fb-capture','build-a04-fb-capture.sh',
            pin,previous.get('capture',{}),'capture')
        result['capture']=capture_result
        text+='#define A04BR3_CAPTURE_PAYLOAD_PRESENT 1\n'
        text+=_array('a04br3_fb_capture_payload',data)
    if ui_test:
        data,ui_result=_build_program(src,cache,'a04-ui-test','build-a04-ui-test.sh',
            pin,previous.get('ui_test',{}),'UI test')
        result['ui_test']=ui_result
        text+='#define A04BR3_UI_TEST_PAYLOAD_PRESENT 1\n'
        text+=_array('a04br3_ui_test_payload',data)
    if keyboard_state:
        data,keyboard_result=_build_program(src,cache,'a04br3-keyboard-state','build-a04br3-keyboard-state.sh',
            pin,previous.get('keyboard_state',{}),'keyboard state')
        result['keyboard_state']=keyboard_result
        text+='#define A04BR3_KEYBOARD_STATE_PAYLOAD_PRESENT 1\n'
        text+=_array('a04br3_keyboard_state_payload',data)
    # Publish only after every requested source build and validation succeeded.
    target=Path(work_source)/'bsd/dev/a04br3_console_payload.inc'
    if not target.is_file() or target.read_text()!=text:target.write_text(text)
    if previous!=result:meta.write_text(json.dumps(result,indent=2)+'\n')
    return result
