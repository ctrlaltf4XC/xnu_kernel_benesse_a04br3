# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Pinned second-program staging without network, SDK or local source inputs."""
import itertools,hashlib,importlib.util,io,json,struct,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('capture_stage',ROOT/'build-support/uart_console.py')
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)

class Staging(unittest.TestCase):
 def fixture(self,base,capture=True,ui_test=False,keyboard_state=False):
  src={'tools/uart-console.c':b'console source','tools/build-uart-console.sh':b'console build','tools/uart-libSystem.tbd':b'link stub'}
  if capture:src.update({name:('source '+name).encode() for name in stage.CAPTURE_FILES})
  if ui_test:src.update({name:('source '+name).encode() for name in stage.UI_TEST_FILES})
  if keyboard_state:src.update({name:('source '+name).encode() for name in stage.KEYBOARD_STATE_FILES})
  pin={'github_repository':'test/source','commit':'a'*40,'files':{k:hashlib.sha256(v).hexdigest() for k,v in src.items()}}
  (base/'uart-console-source.json').write_text(json.dumps(pin))
  work=base/'work';(work/'bsd/dev').mkdir(parents=True,exist_ok=True)
  return src,pin,work
 def fetch(self,sources):
  def run(url,timeout):
   self.assertEqual(timeout,60);name='/'.join(url.split('/')[-2:]);return io.BytesIO(sources[name])
  return run
 def builder(self,commands,fail=False,bad=False):
  def run(cmd,check):
   commands.append(cmd);self.assertTrue(check)
   if 'capture' in cmd[1] and fail:raise subprocess.CalledProcessError(1,cmd)
   data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,0,0,0,0)+Path(cmd[1]).name.encode()
   if 'capture' in cmd[1] and bad:data=b'bad binary'
   Path(cmd[2]).write_bytes(data)
  return run
 def test_two_programs_pin_cache_tamper_and_no_placeholder(self):
  with tempfile.TemporaryDirectory() as d:
   base=Path(d);src,pin,work=self.fixture(base);commands=[]
   with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)) as fetch,patch.object(stage.subprocess,'run',side_effect=self.builder(commands)):
    result=stage.stage_console(work,base/'cache');self.assertEqual(len(commands),2)
    text=(work/'bsd/dev/a04br3_console_payload.inc').read_text();self.assertIn('a04br3_fb_capture_payload[]',text)
    self.assertIn('#define A04BR3_CAPTURE_PAYLOAD_PRESENT 1',text);self.assertIn('capture',result)
    self.assertTrue(all('/'+pin['commit']+'/' in call.args[0] for call in fetch.call_args_list))
    before=(work/'bsd/dev/a04br3_console_payload.inc').stat().st_mtime_ns
    self.assertEqual(stage.stage_console(work,base/'cache'),result);self.assertEqual(len(commands),2)
    self.assertEqual((work/'bsd/dev/a04br3_console_payload.inc').stat().st_mtime_ns,before)
    cache=base/'cache'/('uart-console-'+pin['commit']);(cache/'a04-fb-capture').write_bytes(b'tampered')
    stage.stage_console(work,base/'cache');self.assertEqual(len(commands),3);self.assertIn('build-a04-fb-capture.sh',commands[-1][1])
 def test_old_pin_keeps_console_only(self):
  with tempfile.TemporaryDirectory() as d:
   base=Path(d);src,pin,work=self.fixture(base,False);commands=[]
   with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=self.builder(commands)):
    result=stage.stage_console(work,base/'cache');self.assertEqual(len(commands),1);self.assertNotIn('capture',result)
    self.assertNotIn('CAPTURE_PAYLOAD_PRESENT',(work/'bsd/dev/a04br3_console_payload.inc').read_text())
 def test_partial_pin_or_digest_mismatch_fails_before_build(self):
  with tempfile.TemporaryDirectory() as d:
   base=Path(d);src,pin,work=self.fixture(base)
   del pin['files']['tools/a04-fb-capture-digest.h'];(base/'uart-console-source.json').write_text(json.dumps(pin))
   with patch.object(stage,'SUPPORT',base),patch.object(stage.subprocess,'run') as run:
    with self.assertRaisesRegex(ValueError,'incomplete pinned'):stage.stage_console(work,base/'cache')
    run.assert_not_called()
   src,pin,work=self.fixture(base);src['tools/a04-fb-capture.c']=b'wrong'
   with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run') as run:
    with self.assertRaisesRegex(ValueError,'source mismatch'):stage.stage_console(work,base/'cache')
    run.assert_not_called()
 def test_capture_build_failure_never_publishes_partial_include(self):
  for bad in (False,True):
   with tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base);target=work/'bsd/dev/a04br3_console_payload.inc';target.write_text('previous valid include')
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=self.builder([],fail=not bad,bad=bad)):
     with self.assertRaises((ValueError,subprocess.CalledProcessError)):stage.stage_console(work,base/'cache')
    self.assertEqual(target.read_text(),'previous valid include')
    self.assertFalse((base/'cache'/('uart-console-'+pin['commit'])/'build.json').exists())
 def test_optional_ui_program_cache_and_legacy_capture_independence(self):
  for capture in (False,True):
   with tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base,capture,True);commands=[]
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=self.builder(commands)):
     result=stage.stage_console(work,base/'cache');count=2+int(capture)
     self.assertEqual(len(commands),count);self.assertEqual(result['ui_test']['pin'],pin)
     text=(work/'bsd/dev/a04br3_console_payload.inc').read_text()
     self.assertIn('#define A04BR3_UI_TEST_PAYLOAD_PRESENT 1',text)
     self.assertIn('a04br3_ui_test_payload[]',text)
     self.assertEqual(stage.stage_console(work,base/'cache'),result);self.assertEqual(len(commands),count)
     (base/'cache'/('uart-console-'+pin['commit'])/'a04-ui-test').write_bytes(b'tampered')
     stage.stage_console(work,base/'cache');self.assertEqual(len(commands),count+1)
     self.assertIn('build-a04-ui-test.sh',commands[-1][1])
 def test_ui_partial_pin_and_failed_build_do_not_publish(self):
  with tempfile.TemporaryDirectory() as d:
   base=Path(d);src,pin,work=self.fixture(base,True,True)
   del pin['files']['tools/build-a04-ui-test.sh'];(base/'uart-console-source.json').write_text(json.dumps(pin))
   with patch.object(stage,'SUPPORT',base),patch.object(stage.subprocess,'run') as run:
    with self.assertRaisesRegex(ValueError,'incomplete pinned UI'):stage.stage_console(work,base/'cache')
    run.assert_not_called()
  for invalid in (False,True):
   with tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base,True,True)
    target=work/'bsd/dev/a04br3_console_payload.inc';target.write_text('previous valid include')
    normal=self.builder([])
    def builder(cmd,check):
     if 'build-a04-ui-test.sh' in cmd[1]:
      if invalid:Path(cmd[2]).write_bytes(b'bad binary');return
      raise subprocess.CalledProcessError(1,cmd)
     normal(cmd,check)
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=builder):
     with self.assertRaises((ValueError,subprocess.CalledProcessError)):stage.stage_console(work,base/'cache')
    self.assertEqual(target.read_text(),'previous valid include')
    self.assertFalse((base/'cache'/('uart-console-'+pin['commit'])/'build.json').exists())

 def test_keyboard_all_optional_combinations_and_cache_tamper(self):
  for capture,ui,keyboard in itertools.product((False,True),repeat=3):
   with self.subTest(capture=capture,ui=ui,keyboard=keyboard),tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base,capture,ui,keyboard);commands=[]
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=self.builder(commands)):
     result=stage.stage_console(work,base/'cache');count=1+int(capture)+int(ui)+int(keyboard)
     self.assertEqual(len(commands),count)
     text=(work/'bsd/dev/a04br3_console_payload.inc').read_text()
     self.assertEqual('a04br3_keyboard_state_payload[]' in text,keyboard)
     self.assertEqual('A04BR3_KEYBOARD_STATE_PAYLOAD_PRESENT' in text,keyboard)
     self.assertEqual('keyboard_state' in result,keyboard)
     self.assertEqual(stage.stage_console(work,base/'cache'),result);self.assertEqual(len(commands),count)
     if keyboard:
      cache=base/'cache'/('uart-console-'+pin['commit'])
      (cache/'a04br3-keyboard-state').write_bytes(b'tamper')
      stage.stage_console(work,base/'cache');self.assertEqual(len(commands),count+1)
      self.assertIn('build-a04br3-keyboard-state.sh',commands[-1][1])
      source=cache/'tools/a04br3_keyboard_state.c';source.write_bytes(b'tamper')
      stage.stage_console(work,base/'cache')
      self.assertEqual(source.read_bytes(),src['tools/a04br3_keyboard_state.c'])
      self.assertEqual(len(commands),count+1)

 def test_keyboard_every_partial_group_fails_before_fetch_or_build(self):
  for missing in stage.KEYBOARD_STATE_FILES:
   with self.subTest(missing=missing),tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base,True,True,True)
    del pin['files'][missing];(base/'uart-console-source.json').write_text(json.dumps(pin))
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen') as fetch,patch.object(stage.subprocess,'run') as run:
     with self.assertRaisesRegex(ValueError,'incomplete pinned keyboard'):stage.stage_console(work,base/'cache')
     fetch.assert_not_called();run.assert_not_called()

 def test_keyboard_tamper_or_failed_build_preserves_previous_include_and_metadata(self):
  for fault in ('build','binary','source'):
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as d:
    base=Path(d);src,pin,work=self.fixture(base,True,True,True);normal=self.builder([])
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=normal):
     stage.stage_console(work,base/'cache')
    cache=base/'cache'/('uart-console-'+pin['commit']);target=work/'bsd/dev/a04br3_console_payload.inc'
    before=target.read_bytes();metadata=(cache/'build.json').read_bytes()
    (cache/'a04br3-keyboard-state').write_bytes(b'bad cache')
    if fault=='source':
     (cache/'tools/a04br3_keyboard_state.c').write_bytes(b'bad cache')
     src['tools/a04br3_keyboard_state.c']=b'wrong downloaded source'
    def builder(cmd,check):
     if 'build-a04br3-keyboard-state.sh' in cmd[1]:
      if fault=='binary':Path(cmd[2]).write_bytes(b'not arm64 Mach-O');return
      raise subprocess.CalledProcessError(1,cmd)
     normal(cmd,check)
    with patch.object(stage,'SUPPORT',base),patch.object(stage.urllib.request,'urlopen',side_effect=self.fetch(src)),patch.object(stage.subprocess,'run',side_effect=builder):
     with self.assertRaises((ValueError,subprocess.CalledProcessError)):stage.stage_console(work,base/'cache')
    self.assertEqual(target.read_bytes(),before);self.assertEqual((cache/'build.json').read_bytes(),metadata)

if __name__=='__main__':unittest.main()
