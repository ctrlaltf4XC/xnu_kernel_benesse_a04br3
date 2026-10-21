#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Optional, validated A04BR3 work cache; never a substitute for a source build."""
import argparse
from contextlib import contextmanager
import ctypes
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import posixpath
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile

from incremental import dependency_source_digest, source_files

SUPPORT = Path(__file__).resolve().parent
ARCHIVE = 'kernel-work.tar.xz'
LEGACY_COMMIT = '9ac070452dd3745fdb704b81beaabc5dbc181768'
LEGACY_OUT = '/Users/runner/work/xnu_kernel_benesse_a04br3/xnu_kernel_benesse_a04br3/out/kernel'
# git show 9ac0704:build-support/{build.py,incremental.py,driver.sh}; source
# provenance, not an assumption that a later build recipe is compatible.
LEGACY_RECIPES = {
    'build.py': '039f4723ef39666e86088017726a973ae79cd17bbc38a8049853580c92a48e78',
    'incremental.py': '69e54abb6eb32b9e71e7ac79e4a1cebd44268619d71f9362767b548249dddc35',
    'driver.sh': 'd380f20d97e0cee9d1ba82c80521be919747ba797d68193e407711d09d723ad6',
}
MAX_METADATA = 8 * 1024 * 1024
MAX_MEMBERS = 200000
MAX_EXPANDED = 32 * 1024**3


class CacheError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise CacheError(message)


def sha(value, size=64):
    require(type(value) is str and re.fullmatch('[0-9a-f]{'+str(size)+'}', value), 'invalid digest/commit')
    return value


def integer(value):
    require(type(value) is int and value >= 0, 'invalid nonnegative integer')
    return value


def absolute(value):
    require(type(value) is str and '\0' not in value and value.startswith('/') and
            str(PurePosixPath(value)) == value and '..' not in PurePosixPath(value).parts,
            'output/developer path must be a normalized absolute path')
    return value


def relative(value):
    require(type(value) is str and value and '\0' not in value and '\\' not in value,
            'invalid archive/source name')
    p = PurePosixPath(value)
    require(not p.is_absolute() and all(x not in ('', '.', '..') for x in value.split('/')),
            'noncanonical relative name')
    return value


def read_json_bytes(data):
    require(len(data) <= MAX_METADATA, 'metadata too large')
    def pairs(items):
        out = {}
        for key, value in items:
            require(key not in out, 'duplicate JSON key')
            out[key] = value
        return out
    try:
        return json.loads(data, object_pairs_hook=pairs)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise CacheError('invalid JSON') from error


def file_hash(stream):
    h = hashlib.sha256()
    for block in iter(lambda: stream.read(1024*1024), b''):
        h.update(block)
    return h.hexdigest()


@dataclass(frozen=True)
class Environment:
    system: str
    machine: str
    xcode: str
    sdk_version: str
    developer_dir: str

    @classmethod
    def parse(cls, data):
        require(type(data) is dict and set(data) == set(cls.__annotations__), 'environment schema')
        require(all(type(v) is str for v in data.values()), 'environment field type')
        value = cls(**data)
        require(value.system == 'Darwin' and value.machine == 'arm64', 'Darwin arm64 required')
        require(value.xcode.splitlines()[0:1] == ['Xcode 15.4'] and
                re.fullmatch(r'Xcode 15\.4\nBuild version [A-Za-z0-9]+', value.xcode), 'Xcode15.4 full build required')
        require(re.fullmatch(r'\d+\.\d+(?:\.\d+)?', value.sdk_version), 'SDK version')
        absolute(value.developer_dir)
        return value


@dataclass(frozen=True)
class ArchiveDescriptor:
    name: str
    bytes: int
    sha256: str

    @classmethod
    def parse(cls, data):
        require(type(data) is dict and set(data) == set(cls.__annotations__), 'archive descriptor schema')
        require(data['name'] == ARCHIVE, 'fixed archive filename required')
        require(0 < integer(data['bytes']) <= 8*1024**3, 'compressed size limit')
        sha(data['sha256'])
        return cls(**data)


@dataclass(frozen=True)
class Manifest:
    schema: int
    optional_cache_only: bool
    source_commit: str
    out_absolute: str
    environment: Environment
    compatibility: dict
    archive: ArchiveDescriptor

    @classmethod
    def parse(cls, data):
        require(type(data.get('schema')) is int and data['schema'] == 1 and
                data.get('optional_cache_only') is True, 'optional cache schema1 required')
        sha(data.get('source_commit'), 40);absolute(data.get('out_absolute'))
        require(type(data.get('compatibility')) is dict, 'compatibility object required')
        return cls(1, True, data['source_commit'], data['out_absolute'],
                   Environment.parse(data.get('environment')), data['compatibility'],
                   ArchiveDescriptor.parse(data.get('archive')))


def current_environment():
    def output(*command):
        return subprocess.check_output(command, text=True).strip()
    require(platform.system() == 'Darwin' and platform.machine() == 'arm64', 'restore/export needs actual Darwin arm64')
    return Environment.parse(dict(system=platform.system(), machine=platform.machine(),
        xcode=output('xcodebuild', '-version'),
        sdk_version=output('xcrun', '--sdk', 'macosx', '--show-sdk-version'),
        developer_dir=os.environ.get('DEVELOPER_DIR') or output('xcode-select', '-p')))


def current_compatibility():
    return dict(sdk=read_json_bytes((SUPPORT/'sdk.json').read_bytes()),
                dependencies=read_json_bytes((SUPPORT/'dependencies.json').read_bytes()),
                dependency_source_sha256=dependency_source_digest(),
                recipes={n:hashlib.sha256((SUPPORT/n).read_bytes()).hexdigest() for n in LEGACY_RECIPES})


def check_metadata(data, compatibility, names):
    require(type(data) is dict, 'build metadata object required')
    sha(data.get('source_commit'), 40)
    require(type(data.get('full_kernel_built')) is bool, 'build completion type')
    for field in ('sdk', 'dependencies', 'dependency_source_sha256'):
        require(data.get(field) == compatibility[field], 'incompatible build '+field)
    require(data.get('source_sync_version') == 2, 'source sync version2 required')
    files = data.get('copied_source_files')
    require(type(files) is list and files and all(type(n) is str for n in files) and len(files) == len(set(files)), 'copied source manifest required')
    for name in files:
        relative(name)
        require('kernel/work/xnu/'+name in names, 'copied source missing from cache: '+name)
    require(type(data.get('xcode')) is str and data['xcode'].strip().splitlines()[0:1] == ['Xcode 15.4'], 'metadata Xcode mismatch')


def inventory(members):
    require(len(members) <= MAX_MEMBERS, 'too many members')
    names, parents = {}, set()
    counts = dict(regular=0, directories=0, symlinks=0, hardlinks=0, bytes=0)
    for m in members:
        name = m.name.rstrip('/') if m.isdir() else m.name
        relative(name)
        require(name == 'kernel' or name.startswith('kernel/'), 'outside kernel archive root')
        require(name not in names, 'duplicate archive member')
        require(m.isreg() or m.isdir() or m.issym() or m.islnk(), 'special archive member')
        require(not m.sparse, 'sparse archive member unsupported')
        integer(m.size)
        for parent in PurePosixPath(name).parents:
            p = parent.as_posix()
            if p == '.': break
            require(p not in names or names[p].isdir(), 'member below non-directory')
            parents.add(p)
        require(m.isdir() or name not in parents, 'non-directory replaces member parent')
        if m.issym() or m.islnk():
            require(m.linkname and not m.linkname.startswith('/') and '\\' not in m.linkname and '\0' not in m.linkname,
                    'absolute/invalid link')
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), m.linkname) if m.issym() else m.linkname)
            require(target == 'kernel' or target.startswith('kernel/'), 'link escapes kernel')
            if m.islnk():
                require(target in names and (names[target].isreg() or names[target].islnk()), 'hardlink needs earlier regular target')
        kind = 'regular' if m.isreg() else 'directories' if m.isdir() else 'symlinks' if m.issym() else 'hardlinks'
        counts[kind] += 1
        counts['bytes'] += m.size if m.isreg() else 0
        require(counts['bytes'] <= MAX_EXPANDED, 'expanded cache too large')
        names[name] = m
    require('kernel/build.json' in names and names['kernel/build.json'].isreg(), 'regular build.json required')
    require('kernel/work' in names and names['kernel/work'].isdir(), 'work directory required')
    return names, counts


def safe_filter(member, destination):
    require(callable(getattr(tarfile, 'data_filter', None)), 'Python with tarfile.data_filter required (no permissive fallback)')
    clean = tarfile.data_filter(member, destination)
    root = (Path(destination)/'kernel').resolve()
    target = Path(destination)/clean.name
    require(target.resolve().is_relative_to(root), 'resolved member outside kernel')
    if clean.issym():
        require((target.parent/clean.linkname).resolve().is_relative_to(root), 'resolved symlink outside kernel')
    elif clean.islnk():
        require((Path(destination)/clean.linkname).resolve().is_relative_to(root), 'resolved hardlink outside kernel')
    return clean


def build_request(tf, members, out_absolute):
    requests = [m for m in members if m.name.endswith('/build-request.json') and
                '/libfirehose_kernel.obj/' in m.name and m.isreg()]
    require(len(requests) == 1 and requests[0].size <= MAX_METADATA, 'build request evidence missing')
    request = read_json_bytes(tf.extractfile(requests[0]).read())
    require(type(request) is dict, 'build request object')
    table = request['parameters']['overrides']['commandLine']['table']
    require(type(table) is dict and all(type(table.get(k)) is str for k in ('DSTROOT','OBJROOT','ARCHS','VALID_ARCHS','SDKROOT')), 'build request field type')
    absolute(request.get('containerPath'));absolute(table['DSTROOT']);absolute(table['OBJROOT'])
    require(request['containerPath'].startswith(out_absolute+'/work/') and
            table['DSTROOT'] == out_absolute+'/work/fakeroot' and
            table['OBJROOT'].startswith(out_absolute+'/work/build/') and
            table['ARCHS'] == table['VALID_ARCHS'] == 'arm64', 'path/target architecture evidence mismatch')
    require(type(table['SDKROOT']) is str and table['SDKROOT'].startswith('macosx'), 'SDK evidence missing')
    return table


def legacy_manifest(raw, metadata, tf, members, compatibility):
    # Only the explicitly identified recovered cache. Never rewrite manifest.json.
    require(raw.get('optional_cache_only') is True and raw.get('source_commit') == LEGACY_COMMIT,
            'unknown legacy cache context')
    require(metadata['source_commit'] == LEGACY_COMMIT, 'legacy source mismatch')
    require(compatibility['recipes'] == LEGACY_RECIPES, 'legacy recipe changed; clean build required')
    table = build_request(tf, members, LEGACY_OUT)
    env = Environment.parse(dict(system='Darwin', machine='arm64', xcode=metadata['xcode'].strip(),
        sdk_version=table['SDKROOT'][6:], developer_dir='/Applications/Xcode_15.4.app/Contents/Developer'))
    # Darwin/host-arm64/developer path are explicit retained runner context;
    # target ARCHS alone is not claimed to prove host architecture.
    return dict(schema=1, optional_cache_only=True, source_commit=raw['source_commit'],
                out_absolute=LEGACY_OUT, environment=asdict(env), compatibility=compatibility,
                archive=dict(name=ARCHIVE, bytes=raw['bytes'], sha256=raw['sha256']),
                legacy_evidence='retained9ac runner context + archive build.json and libfirehose build-request')


@contextmanager
def checked_cache(cache, compatibility, legacy=False):
    raw = read_json_bytes((cache/'manifest.json').read_bytes())
    require(type(raw) is dict, 'manifest object required')
    desc = raw if legacy else raw.get('archive', {})
    require(type(desc) is dict, 'archive descriptor')
    expected_bytes, expected_sha = integer(desc.get('bytes')), sha(desc.get('sha256'))
    require(0 < expected_bytes <= 8*1024**3, 'compressed size limit')
    if not legacy: require(desc.get('name') == ARCHIVE, 'fixed archive filename required')
    fd = os.open(cache/ARCHIVE, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, 'rb') as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode) and os.fstat(stream.fileno()).st_size == expected_bytes, 'archive size/type mismatch')
        require(file_hash(stream) == expected_sha, 'archive SHA256 mismatch')
        stream.seek(0)
        with tarfile.open(fileobj=stream, mode='r:xz') as tf:
            members = tf.getmembers()
            names, counts = inventory(members)
            require(names['kernel/build.json'].size <= MAX_METADATA, 'build metadata too large')
            metadata = read_json_bytes(tf.extractfile(names['kernel/build.json']).read())
            check_metadata(metadata, compatibility, names)
            manifest = legacy_manifest(raw, metadata, tf, members, compatibility) if legacy else raw
            require(type(manifest.get('schema')) is int and manifest['schema'] == 1 and manifest.get('optional_cache_only') is True, 'optional cache schema1 required')
            sha(manifest.get('source_commit'), 40)
            require(manifest['source_commit'] == metadata['source_commit'], 'manifest source mismatch')
            absolute(manifest.get('out_absolute'))
            decoded = Manifest.parse(manifest)
            env = decoded.environment
            require(env.xcode == metadata['xcode'].strip(), 'manifest/build Xcode mismatch')
            request = build_request(tf, members, manifest['out_absolute'])
            require(request['SDKROOT'] == 'macosx'+env.sdk_version, 'manifest/build SDK version mismatch')
            require(manifest.get('compatibility') == compatibility, 'cache SDK/dependency/recipe incompatibility')
            # Offline datafilter pass checks every member, including all links.
            with tempfile.TemporaryDirectory(prefix='a04-cache-check-') as scratch:
                for m in members: safe_filter(m, scratch)
            yield manifest, metadata, tf, members, counts


def atomic_publish(source, destination):
    """No replacement, even of a raced-in empty directory. No unsafe fallback."""
    libc = ctypes.CDLL(None, use_errno=True)
    if platform.system() == 'Darwin':
        fn = libc.renamex_np
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        args = (os.fsencode(source), os.fsencode(destination), 4) # XNU sys/stdio.h RENAME_EXCL
    elif platform.system() == 'Linux' and hasattr(libc, 'renameat2'):
        fn = libc.renameat2
        fn.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        args = (-100, os.fsencode(source), -100, os.fsencode(destination), 1) # AT_FDCWD, RENAME_NOREPLACE
    else:
        raise CacheError('atomic no-replace rename unavailable')
    fn.restype = ctypes.c_int
    if fn(*args):
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code), str(destination))


def target_path(path):
    require(path.is_absolute() and str(path) == absolute(str(path)), 'absolute target required')
    require(path.parent.is_dir() and path.parent.resolve() == path.parent, 'canonical existing parent required')
    require(not os.path.lexists(path), 'target must not exist, including an empty dir/symlink')


def restore(cache, out, compatibility, environment, legacy=False):
    target_path(out)
    with checked_cache(cache, compatibility, legacy) as (manifest, metadata, tf, members, counts):
        require(str(out) == manifest['out_absolute'], 'absolute output path changed; no relocation')
        require(environment == Environment.parse(manifest['environment']), 'actual runner environment mismatch')
        with tempfile.TemporaryDirectory(prefix='.a04-cache-restore-', dir=out.parent) as scratch:
            tf.extractall(scratch, members=members, filter=safe_filter)
            restored = Path(scratch)/'kernel'
            # Never let an old cached binary be reported as a fresh SDK build.
            metadata['full_kernel_built'] = False
            metadata.pop('kernel_sha256', None)
            metadata['cache_restored_from'] = dict(source_commit=manifest['source_commit'],
                archive_sha256=manifest['archive']['sha256'], out_absolute=manifest['out_absolute'],
                environment=manifest['environment'], legacy=legacy)
            (restored/'build.json').write_text(json.dumps(metadata, indent=2)+'\n')
            artifact = restored/'kernel.development.a04br3'
            if artifact.exists() or artifact.is_symlink(): artifact.unlink()
            atomic_publish(restored, out)
        return dict(restored=True, source_commit=manifest['source_commit'], full_kernel_built=False, counts=counts)


def verify_export_source(root, out, declared_commit=None):
    """Git top only, or explicitly identified copied source; never parent Git."""
    root = root.resolve()
    if declared_commit is not None: sha(declared_commit, 40)
    metadata = read_json_bytes((out/'build.json').read_bytes())
    require(type(metadata) is dict, 'build metadata object required')
    commit = sha(metadata.get('source_commit'), 40)
    try:
        git = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=root,
                             capture_output=True, text=True)
    except FileNotFoundError:
        git = None
    real_git_top = (git is not None and git.returncode == 0 and
                    Path(git.stdout.strip()).resolve() == root)
    if real_git_top:
        head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
        dirty = subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True)
        require(not dirty, 'export needs clean HEAD when ROOT is the real Git top')
        require(sha(head, 40) == commit, 'export source/build commit mismatch')
        require(declared_commit is None or declared_commit == head, '--source-commit disagrees with HEAD')
        mode = 'git-top'
    else:
        require(declared_commit is not None, 'copied/Gitless export requires explicit --source-commit (40hex)')
        require(declared_commit == commit, 'copied source/build commit mismatch')
        mode = 'copied-source'
    # Compare only source_files(ROOT), exactly as the existing copier does.
    # Generated payloads/header outputs absent from that manifest stay outside
    # this comparison, while every declared source input must match exactly.
    current = source_files(root)
    previous = metadata.get('copied_source_files')
    require(type(previous) is list and all(type(n) is str for n in previous) and
            len(previous) == len(set(previous)) and sorted(previous) == current,
            'current/copied source manifest mismatch')
    copied = out/'work/xnu'
    require(copied.is_dir() and not copied.is_symlink() and copied.resolve() == copied,
            'canonical copied source directory required')
    for name in current:
        relative(name)
        src, dst = root/name, copied/name
        parent = dst.parent
        while parent != copied:
            require(parent.is_dir() and not parent.is_symlink(), 'copied source parent differs: '+name)
            parent = parent.parent
        source_stat = src.lstat()
        if stat.S_ISLNK(source_stat.st_mode):
            require(dst.is_symlink() and os.readlink(src) == os.readlink(dst), 'copied symlink differs: '+name)
        else:
            require(stat.S_ISREG(source_stat.st_mode) and not dst.is_symlink() and dst.is_file(),
                    'copied regular input differs: '+name)
            with src.open('rb') as left, dst.open('rb') as right:
                ls, rs = os.fstat(left.fileno()), os.fstat(right.fileno())
                require(stat.S_ISREG(ls.st_mode) and stat.S_ISREG(rs.st_mode) and
                        ls.st_size == rs.st_size and stat.S_IMODE(ls.st_mode) == stat.S_IMODE(rs.st_mode),
                        'copied input size/mode differs: '+name)
                while True:
                    a, b = left.read(1024*1024), right.read(1024*1024)
                    require(a == b, 'copied input bytes differ: '+name)
                    if not a: break
    return dict(mode=mode, source_commit=commit, files=len(current))


def export(out, cache, compatibility, environment, *, source_root=None, source_commit=None):
    require(out.is_absolute() and out.resolve() == out and out.is_dir(), 'canonical source out required')
    target_path(cache)
    require(not cache.is_relative_to(out), 'cache cannot be inside source output')
    proof = verify_export_source(source_root, out, source_commit) if source_root is not None else None
    metadata_bytes = (out/'build.json').read_bytes()
    metadata = read_json_bytes(metadata_bytes)
    require(metadata.get('full_kernel_built') is True, 'export only a finished build (not concurrent build)')
    require(metadata.get('xcode', '').strip() == environment.xcode, 'build/actual Xcode mismatch')
    with tempfile.TemporaryDirectory(prefix='.a04-cache-export-', dir=cache.parent) as scratch:
        stage = Path(scratch)/'cache';stage.mkdir()
        archive = stage/ARCHIVE
        with tarfile.open(archive, 'w:xz', dereference=False) as tf:
            tf.add(out/'build.json', arcname='kernel/build.json')
            tf.add(out/'work', arcname='kernel/work')
        with archive.open('rb') as stream: digest = file_hash(stream)
        manifest = dict(schema=1, optional_cache_only=True, source_commit=metadata['source_commit'],
            out_absolute=str(out), environment=asdict(environment), compatibility=compatibility,
            archive=dict(name=ARCHIVE, bytes=archive.stat().st_size, sha256=digest))
        if proof is not None: manifest['export_source_verified'] = proof
        (stage/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        with checked_cache(stage, compatibility): pass
        require((out/'build.json').read_bytes() == metadata_bytes, 'build changed during export; retry when quiescent')
        if proof is not None:
            require(verify_export_source(source_root, out, source_commit) == proof, 'source changed during export')
        atomic_publish(stage, cache)
    return dict(exported=True, source_commit=manifest['source_commit'], archive=manifest['archive'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['export', 'validate', 'restore'])
    parser.add_argument('--cache', required=True, type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--source-commit', help='explicit 40hex source identity required for copied/Gitless export')
    parser.add_argument('--legacy', action='store_true', help='read-only normalization of known recovered9ac cache')
    args = parser.parse_args()
    try:
        compatibility = current_compatibility()
        if args.command == 'validate':
            with checked_cache(args.cache, compatibility, args.legacy) as (m, _, _, _, counts):
                result = dict(validated=True, host_restore_tested=False, manifest=m, counts=counts)
        else:
            require(args.out is not None, '--out required')
            environment = current_environment()
            if args.command == 'restore': result = restore(args.cache, args.out, compatibility, environment, args.legacy)
            else:
                require(not args.legacy, '--legacy only for validate/restore')
                result = export(args.out, args.cache, compatibility, environment,
                                source_root=SUPPORT.parent, source_commit=args.source_commit)
        print(json.dumps(result, indent=2))
    except (CacheError, OSError, tarfile.TarError, KeyError, TypeError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(2, 'optional work cache rejected: '+str(error)+'\n')


if __name__ == '__main__': main()
