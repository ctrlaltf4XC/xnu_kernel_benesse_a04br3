# SPDX-License-Identifier: MIT
"""Actual export/filter/restore using tiny host fixtures, no SDK/runner needed.
Darwin environment values are explicitly TEST INPUT on non-Darwin hosts.
Actual 233MB legacy cache is inspected separately by CLI, not a test dependency.
"""
from dataclasses import asdict, replace
import hashlib
import io
from contextlib import redirect_stdout, redirect_stderr
import json
import os
import shutil
import subprocess
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import work_cache as cache

ENV = cache.Environment.parse(dict(system='Darwin', machine='arm64',
    xcode='Xcode 15.4\nBuild version 15F31d', sdk_version='14.5',
    developer_dir='/Applications/Xcode_15.4.app/Contents/Developer'))
COMPAT = dict(sdk={'xcode_version':'15.4','sdk':'macosx'},
              dependencies={'projects':[{'name':'fixture','commit':'b'*40}]},
              dependency_source_sha256='c'*64, recipes={'driver.sh':'d'*64})

class WorkCache(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
  self.root=Path(self.temp.name).resolve();self.out=self.root/'kernel';self.store=self.root/'cache'
  self.out.mkdir();work=self.out/'work';(work/'xnu').mkdir(parents=True)
  (work/'xnu/unit.c').write_bytes(b'actual test input\n')
  (work/'xnu/header.h').symlink_to('unit.c')
  (work/'fakeroot').mkdir();(work/'fakeroot/tool').write_bytes(b'test-tool');(work/'fakeroot/tool').chmod(0o755)
  os.link(work/'fakeroot/tool',work/'fakeroot/hardtool')
  (work/'generated-tools').mkdir();(work/'generated-tools/cache-marker').write_text('retain generated tools')
  self.metadata=dict(source_commit='a'*40,xcode=ENV.xcode+'\n',full_kernel_built=True,kernel_sha256='e'*64,
    sdk=COMPAT['sdk'],dependencies=COMPAT['dependencies'],dependency_source_sha256=COMPAT['dependency_source_sha256'],
    copied_source_files=['unit.c','header.h'],source_sync_version=2)
  (self.out/'build.json').write_text(json.dumps(self.metadata))
  self.request_path=work/'build/libfirehose_kernel.obj/XCBuildData/test/build-request.json'
  self.request_path.parent.mkdir(parents=True)
  request=dict(containerPath=str(self.out)+'/work/libdispatch/test.xcodeproj',parameters=dict(overrides=dict(commandLine=dict(table=dict(
    DSTROOT=str(self.out)+'/work/fakeroot',OBJROOT=str(self.out)+'/work/build/test',ARCHS='arm64',VALID_ARCHS='arm64',SDKROOT='macosx14.5')))))
  self.request_path.write_text(json.dumps(request))

 def export_fixture(self):
  cache.export(self.out,self.store,COMPAT,ENV)
  self.out.rename(self.root/'original')
 def load(self):return json.loads((self.store/'manifest.json').read_text())
 def save(self,m): (self.store/'manifest.json').write_text(json.dumps(m))
 def rewrite_archive(self,extra=None,mutate=None):
  path=self.store/cache.ARCHIVE
  with tarfile.open(path,'r:xz') as tf:
   entries=[(m,tf.extractfile(m).read() if m.isreg() else None) for m in tf.getmembers()]
  if mutate:entries=mutate(entries)
  if extra:entries.extend(extra)
  with tarfile.open(path,'w:xz') as tf:
   for m,data in entries:tf.addfile(m,io.BytesIO(data) if data is not None else None)
  manifest=self.load();manifest['archive']['bytes']=path.stat().st_size
  manifest['archive']['sha256']=hashlib.sha256(path.read_bytes()).hexdigest();self.save(manifest)
 def rejected_restore(self):
  with self.assertRaises((cache.CacheError,OSError,tarfile.TarError)):
   cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertFalse(os.path.lexists(self.out))
  self.assertFalse(list(self.root.glob('.a04-cache-restore-*')))
 def test_roundtrip_links_metadata_fakeroot_and_generated_tools(self):
  self.export_fixture()
  with cache.checked_cache(self.store,COMPAT) as (_,_,_,_,counts):
   self.assertEqual((counts['symlinks'],counts['hardlinks']),(1,1))
  result=cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertFalse(result['full_kernel_built'])
  got=json.loads((self.out/'build.json').read_text())
  self.assertIs(got['full_kernel_built'],False);self.assertNotIn('kernel_sha256',got)
  self.assertEqual(got['cache_restored_from']['source_commit'],'a'*40)
  self.assertEqual(got['source_commit'],'a'*40);self.assertEqual(got['copied_source_files'],self.metadata['copied_source_files'])
  work=self.out/'work';self.assertEqual(os.readlink(work/'xnu/header.h'),'unit.c')
  self.assertEqual((work/'fakeroot/tool').stat().st_ino,(work/'fakeroot/hardtool').stat().st_ino)
  self.assertTrue((work/'fakeroot/tool').stat().st_mode&0o100)
  self.assertEqual((work/'generated-tools/cache-marker').read_text(),'retain generated tools')
 def test_existing_targets_even_empty_and_symlinks_untouched(self):
  self.export_fixture()
  for kind in ('directory','file','symlink'):
   with self.subTest(kind=kind):
    if kind=='directory':self.out.mkdir()
    elif kind=='file':self.out.write_text('keep')
    else:self.out.symlink_to(self.root/'original',target_is_directory=True)
    with self.assertRaises(cache.CacheError):cache.restore(self.store,self.out,COMPAT,ENV)
    if kind=='directory':self.assertEqual(list(self.out.iterdir()),[]);self.out.rmdir()
    elif kind=='file':self.assertEqual(self.out.read_text(),'keep');self.out.unlink()
    else:self.assertTrue(self.out.is_symlink());self.out.unlink()
 def test_raced_empty_target_not_replaced_atomic(self):
  self.export_fixture();actual=cache.atomic_publish
  def raced(src,dst):dst.mkdir();actual(src,dst)
  with patch.object(cache,'atomic_publish',side_effect=raced):
   with self.assertRaises(FileExistsError):cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertTrue(self.out.is_dir());self.assertEqual(list(self.out.iterdir()),[])
  self.assertFalse(list(self.root.glob('.a04-cache-restore-*')))
 def test_extract_failure_no_partial_publication(self):
  self.export_fixture()
  with patch.object(tarfile.TarFile,'extractall',side_effect=OSError('disk failure')):
   self.rejected_restore()
 def test_corrupt_sha_size_and_xz_rejected(self):
  self.export_fixture();original=self.load()
  for field,value in [('sha256','0'*64),('bytes',True),('bytes',original['archive']['bytes']+1)]:
   with self.subTest(field=field,value=value):
    data=json.loads(json.dumps(original));data['archive'][field]=value;self.save(data);self.rejected_restore()
  path=self.store/cache.ARCHIVE;path.write_bytes(b'not xz')
  original['archive'].update(bytes=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest());self.save(original)
  self.rejected_restore()
 def test_types_host_sdk_dependency_recipe_and_absolute_path(self):
  self.export_fixture();original=self.load()
  for field,value in [('schema',True),('schema',2),('optional_cache_only',1),('source_commit','0'*40),
                      ('out_absolute','relative'),('out_absolute',str(self.root/'another'))]:
   with self.subTest(field=field):
    m=dict(original);m[field]=value;self.save(m);self.rejected_restore()
  for field,value in [('system','Linux'),('machine','x86_64'),('xcode','Xcode 15.4'),('sdk_version','15.0')]:
   with self.subTest(environment=field):
    m=json.loads(json.dumps(original));m['environment'][field]=value;self.save(m);self.rejected_restore()
  self.save(original)
  for field,value in [('dependencies',{}),('sdk',{}),('recipes',{}),('dependency_source_sha256','0'*64)]:
   with self.subTest(compatibility=field):
    changed=dict(COMPAT);changed[field]=value
    with self.assertRaises(cache.CacheError):cache.restore(self.store,self.out,changed,ENV)
  with self.assertRaises(cache.CacheError):cache.restore(self.store,self.out,COMPAT,replace(ENV,sdk_version='15.0'))
 def test_escape_special_duplicate_parent_and_link_rejection(self):
  def member(name,kind=tarfile.REGTYPE,target=''):
   m=tarfile.TarInfo(name);m.type=kind;m.linkname=target;m.size=1 if kind==tarfile.REGTYPE else 0
   return m,b'x' if m.isreg() else None
  extras=[member('../escape'),member('/kernel/escape'),member('other/file'),
          member('kernel/work/pipe',tarfile.FIFOTYPE),member('kernel/work/xnu/unit.c'),
          member('kernel/work/abs',tarfile.SYMTYPE,'/etc'),
          member('kernel/work/up',tarfile.SYMTYPE,'../../escape'),
          member('kernel/work/hard',tarfile.LNKTYPE,'../../etc/passwd'),
          member('kernel/work/xnu/header.h/child'),
          member('kernel/work/forward',tarfile.LNKTYPE,'kernel/work/missing')]
  for extra in extras:
   with self.subTest(name=extra[0].name):
    if self.store.exists():import shutil;shutil.rmtree(self.store)
    cache.export(self.out,self.store,COMPAT,ENV);self.rewrite_archive(extra=[extra])
    absent=self.root/'restore';m=self.load();m['out_absolute']=str(absent);self.save(m)
    with self.assertRaises((cache.CacheError,tarfile.TarError)):
     cache.restore(self.store,absent,COMPAT,ENV)
    self.assertFalse(absent.exists())
 def test_actual_datafilter_called_and_no_permissive_fallback(self):
  self.export_fixture();actual=tarfile.data_filter;seen=[]
  def checked(m,d):seen.append(m.name);return actual(m,d)
  with patch.object(tarfile,'data_filter',side_effect=checked):cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertIn('kernel/work/fakeroot/hardtool',seen);self.assertIn('kernel/work/xnu/header.h',seen)
 def test_missing_copied_source_and_bad_metadata_rejected(self):
  self.export_fixture()
  def mutate(entries):
   result=[]
   for m,b in entries:
    if m.name=='kernel/build.json':
     x=json.loads(b);x['copied_source_files'].append('missing.h');b=json.dumps(x).encode();m.size=len(b)
    result.append((m,b))
   return result
  self.rewrite_archive(mutate=mutate);self.rejected_restore()
 def test_export_does_not_replace_existing_cache_or_accept_unfinished(self):
  cache.export(self.out,self.store,COMPAT,ENV);before=(self.store/'manifest.json').read_bytes()
  with self.assertRaises(cache.CacheError):cache.export(self.out,self.store,COMPAT,ENV)
  self.assertEqual(before,(self.store/'manifest.json').read_bytes())
  self.metadata['full_kernel_built']=False;(self.out/'build.json').write_text(json.dumps(self.metadata))
  with self.assertRaises(cache.CacheError):cache.export(self.out,self.root/'another-cache',COMPAT,ENV)
 def test_legacy_normalization_readonly_source_and_evidence(self):
  # Tiny synthetic legacy format; actual recovered archive validation separate.
  self.metadata['source_commit']=cache.LEGACY_COMMIT;(self.out/'build.json').write_text(json.dumps(self.metadata))
  p=self.out/'work/build/libfirehose_kernel.obj/XCBuildData/test';p.mkdir(parents=True,exist_ok=True)
  request=dict(containerPath=cache.LEGACY_OUT+'/work/libdispatch/test.xcodeproj',parameters=dict(overrides=dict(commandLine=dict(table=dict(
    DSTROOT=cache.LEGACY_OUT+'/work/fakeroot',OBJROOT=cache.LEGACY_OUT+'/work/build/test',ARCHS='arm64',VALID_ARCHS='arm64',SDKROOT='macosx14.5')))))
  self.request_path.write_text(json.dumps(request))
  compat=dict(COMPAT,recipes=cache.LEGACY_RECIPES)
  # Construct the legacy fixture at its recorded absolute build path without
  # restoring there or weakening the actual export path check.
  saved=self.request_path.read_text();local=json.loads(saved);local['containerPath']=str(self.out)+'/work/libdispatch/test.xcodeproj';table=local['parameters']['overrides']['commandLine']['table'];table['DSTROOT']=str(self.out)+'/work/fakeroot';table['OBJROOT']=str(self.out)+'/work/build/test';self.request_path.write_text(json.dumps(local))
  cache.export(self.out,self.store,compat,ENV)
  def legacy_request(entries):
   result=[]
   for m,b in entries:
    if m.name.endswith('/build-request.json'):b=saved.encode();m.size=len(b)
    result.append((m,b))
   return result
  self.rewrite_archive(mutate=legacy_request)
  archive=self.load()['archive'];raw=dict(source_commit=cache.LEGACY_COMMIT,optional_cache_only=True,bytes=archive['bytes'],sha256=archive['sha256'])
  self.save(raw);original=(self.store/'manifest.json').read_bytes()
  with cache.checked_cache(self.store,compat,True) as (normalized,_,_,_,counts):
   self.assertEqual(normalized['out_absolute'],cache.LEGACY_OUT);self.assertEqual(counts['symlinks'],1)
  self.assertEqual(original,(self.store/'manifest.json').read_bytes())
  raw['source_commit']='0'*40;self.save(raw)
  with self.assertRaises(cache.CacheError):
   with cache.checked_cache(self.store,compat,True):pass
 def test_restored_top_kernel_never_becomes_fresh_artifact(self):
  self.export_fixture();m=tarfile.TarInfo('kernel/kernel.development.a04br3');m.size=6
  self.rewrite_archive(extra=[(m,b'cached')]);cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertFalse((self.out/'kernel.development.a04br3').exists())
  self.assertFalse(json.loads((self.out/'build.json').read_text())['full_kernel_built'])
 def test_link_chain_escape_detected_during_actual_filter_extraction(self):
  self.export_fixture()
  a=tarfile.TarInfo('kernel/work/a');a.type=tarfile.SYMTYPE;a.linkname='..'
  b=tarfile.TarInfo('kernel/work/b');b.type=tarfile.SYMTYPE;b.linkname='a/../../escape'
  self.rewrite_archive(extra=[(a,None),(b,None)])
  self.rejected_restore();self.assertFalse((self.root/'escape').exists())
 def test_selected_developer_dir_overrides_global_xcode_select(self):
  def output(command,**kw):
   self.assertNotIn('xcode-select',command)
   return ENV.xcode if command[0]=='xcodebuild' else ENV.sdk_version
  with patch.object(cache.platform,'system',return_value='Darwin'),patch.object(cache.platform,'machine',return_value='arm64'),patch.dict(os.environ,DEVELOPER_DIR=ENV.developer_dir),patch.object(cache.subprocess,'check_output',side_effect=output):
   self.assertEqual(cache.current_environment(),ENV)
 def test_missing_filter_has_no_unsafe_fallback(self):
  self.export_fixture()
  with patch.object(tarfile,'data_filter',None):self.rejected_restore()
 def test_darwin_no_replace_abi_uses_source_defined_excl_flag(self):
  from unittest.mock import Mock
  fn=Mock(return_value=0);lib=Mock(renamex_np=fn)
  with patch.object(cache.platform,'system',return_value='Darwin'),patch.object(cache.ctypes,'CDLL',return_value=lib):
   cache.atomic_publish(self.root/'source',self.root/'destination')
  fn.assert_called_once_with(os.fsencode(self.root/'source'),os.fsencode(self.root/'destination'),4)
 def test_unsupported_atomic_rename_never_falls_back(self):
  with patch.object(cache.platform,'system',return_value='Other'):
   with self.assertRaises(cache.CacheError):cache.atomic_publish(self.root/'source',self.root/'destination')
  self.assertFalse((self.root/'destination').exists())
 def test_driver_fingerprint_changes_but_uart_pin_and_stager_do_not(self):
  support=self.root/'support';support.mkdir()
  (support/'sdk.json').write_text(json.dumps(COMPAT['sdk']))
  (support/'dependencies.json').write_text(json.dumps(COMPAT['dependencies']))
  for n in cache.LEGACY_RECIPES:(support/n).write_text('fixture recipe '+n)
  with patch.object(cache,'SUPPORT',support),patch.object(cache,'dependency_source_digest',return_value=COMPAT['dependency_source_sha256']):
   before=cache.current_compatibility()
   cache.export(self.out,self.store,before,ENV);self.out.rename(self.root/'original')
   (support/'uart-console-source.json').write_text('changed source pin')
   (support/'uart_console.py').write_text('changed payload stager')
   after=cache.current_compatibility();self.assertEqual(before,after)
   cache.restore(self.store,self.out,after,ENV)
   (support/'driver.sh').write_text('different build recipe')
   changed=cache.current_compatibility();self.assertNotEqual(changed['recipes']['driver.sh'],before['recipes']['driver.sh'])
   with self.assertRaises(cache.CacheError):
    with cache.checked_cache(self.store,changed):pass
 def copied_source(self):
  source=self.root/'copied-source'
  if source.exists():shutil.rmtree(source)
  shutil.copytree(self.out/'work/xnu',source,symlinks=True)
  (source/'build-support').mkdir()
  (source/'build-support/uart-console-source.json').write_text('source pin fixture, not a source_files input')
  return source
 def cli_export(self,source,*extra):
  args=['work_cache.py','export','--cache',str(self.store),'--out',str(self.out),*extra]
  stdout,stderr=io.StringIO(),io.StringIO()
  with patch.object(cache,'SUPPORT',source/'build-support'),patch.object(cache,'current_compatibility',return_value=COMPAT),patch.object(cache,'current_environment',return_value=ENV),patch.object(sys,'argv',args),redirect_stdout(stdout),redirect_stderr(stderr):
   cache.main()
  return json.loads(stdout.getvalue())
 def test_gitless_cli_export_success_explicit_commit_and_generated_payload(self):
  source=self.copied_source()
  # Not listed by source_files(ROOT) or copied_source_files: valid staged payload.
  generated=self.out/'work/xnu/bsd/dev/payload.inc';generated.parent.mkdir(parents=True);generated.write_text('source-built payload')
  result=self.cli_export(source,'--source-commit','a'*40)
  self.assertTrue(result['exported'])
  self.assertEqual(self.load()['export_source_verified'],dict(mode='copied-source',source_commit='a'*40,files=2))
  self.out.rename(self.root/'original');cache.restore(self.store,self.out,COMPAT,ENV)
  self.assertEqual((self.out/'work/xnu/bsd/dev/payload.inc').read_text(),'source-built payload')
 def test_gitless_cli_without_commit_invalid_or_metadata_mismatch_refused(self):
  source=self.copied_source()
  for args in [(),('--source-commit','bad'),('--source-commit','b'*40)]:
   with self.subTest(args=args):
    with self.assertRaises(SystemExit) as rejected:self.cli_export(source,*args)
    self.assertEqual(rejected.exception.code,2);self.assertFalse(self.store.exists())
 def test_parent_git_top_is_not_adopted_by_copied_cli(self):
  source=self.copied_source()
  parent=subprocess.CompletedProcess(['git'],0,stdout=str(source.parent)+'\n',stderr='')
  with patch.object(cache.subprocess,'run',return_value=parent) as discover,patch.object(cache.subprocess,'check_output',side_effect=AssertionError('must not use parent HEAD/status')):
   with self.assertRaises(SystemExit):self.cli_export(source)
   self.assertTrue(self.cli_export(source,'--source-commit','a'*40)['exported'])
   self.assertGreaterEqual(discover.call_count,3)
 def test_copied_current_and_work_bytes_links_and_source_list_must_match(self):
  for fault in ('current-bytes','work-bytes','current-link','work-link','current-extra','source-list'):
   with self.subTest(fault=fault):
    work=self.out/'work/xnu';(work/'unit.c').write_bytes(b'actual test input\n')
    (work/'header.h').unlink();(work/'header.h').symlink_to('unit.c')
    (self.out/'build.json').write_text(json.dumps(self.metadata))
    source=self.copied_source()
    if fault=='current-bytes':(source/'unit.c').write_bytes(b'other input')
    if fault=='work-bytes':(work/'unit.c').write_bytes(b'other copied input')
    if fault=='current-link':(source/'header.h').unlink();(source/'header.h').symlink_to('other.h')
    if fault=='work-link':(work/'header.h').unlink();(work/'header.h').symlink_to('other.h')
    if fault=='current-extra':(source/'extra.c').write_text('extra input')
    if fault=='source-list':
     changed=dict(self.metadata,copied_source_files=['unit.c']);(self.out/'build.json').write_text(json.dumps(changed))
    with self.assertRaises(cache.CacheError):cache.verify_export_source(source,self.out,'a'*40)
    self.assertFalse(self.store.exists())
 def test_real_git_top_requires_clean_matching_head_and_explicit_override(self):
  source=self.copied_source();top=subprocess.CompletedProcess(['git'],0,stdout=str(source)+'\n',stderr='')
  for head,dirty,explicit,valid in [('a'*40,'',None,True),('a'*40,' M unit.c',None,False),('b'*40,'',None,False),('a'*40,'','b'*40,False)]:
   with self.subTest(head=head,dirty=dirty,explicit=explicit):
    def query(command,**kw):return head if command[1]=='rev-parse' else dirty
    with patch.object(cache.subprocess,'run',return_value=top),patch.object(cache.subprocess,'check_output',side_effect=query):
     if valid:self.assertEqual(cache.verify_export_source(source,self.out,explicit)['mode'],'git-top')
     else:
      with self.assertRaises(cache.CacheError):cache.verify_export_source(source,self.out,explicit)
 def test_source_changed_during_export_not_published(self):
  source=self.copied_source();actual=tarfile.TarFile.add
  def raced(tf,name,*args,**kwargs):
   result=actual(tf,name,*args,**kwargs)
   if kwargs.get('arcname')=='kernel/work':(source/'unit.c').write_text('changed while compressing')
   return result
  with patch.object(tarfile.TarFile,'add',raced):
   with self.assertRaises(cache.CacheError):cache.export(self.out,self.store,COMPAT,ENV,source_root=source,source_commit='a'*40)
  self.assertFalse(self.store.exists());self.assertFalse(list(self.root.glob('.a04-cache-export-*')))
 def test_current_environment_rejects_non_darwin_before_xcode(self):
  with patch.object(cache.platform,'system',return_value='Linux'),patch.object(cache.subprocess,'check_output') as command:
   with self.assertRaises(cache.CacheError):cache.current_environment()
   command.assert_not_called()

if __name__=='__main__':unittest.main()
