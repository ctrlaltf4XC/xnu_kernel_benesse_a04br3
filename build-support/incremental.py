#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rebuild committed copied source using an existing A04BR3 work directory."""
import argparse
import filecmp
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess

SUPPORT = Path(__file__).resolve().parent
ROOT = SUPPORT.parent
EXCLUDED = {'.git', 'build-support', 'out', '__pycache__'}


def source_files(root):
    files = []
    for directory, dirs, names in os.walk(root):
        dirs[:] = [d for d in dirs if d not in EXCLUDED]
        for name in list(dirs):
            path = Path(directory) / name
            if path.is_symlink():
                files.append(path.relative_to(root).as_posix())
                dirs.remove(name)
        for name in names:
            files.append((Path(directory) / name).relative_to(root).as_posix())
    return sorted(files)


def sync_source(source, destination, previous):
    current = source_files(source)
    for name in set(previous) | set(current):
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('invalid source manifest path')
    for name in set(previous) - set(current):
        target = destination / name
        if target.is_file() or target.is_symlink():
            target.unlink()
    for name in current:
        src, dst = source / name, destination / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.is_symlink():
            if dst.is_symlink() and os.readlink(src) == os.readlink(dst):
                continue
            if dst.exists() or dst.is_symlink():
                dst.unlink()
            dst.symlink_to(os.readlink(src))
        elif not dst.is_file() or dst.is_symlink() or not filecmp.cmp(src, dst, shallow=False) or stat.S_IMODE(src.stat().st_mode) != stat.S_IMODE(dst.stat().st_mode):
            if dst.is_symlink():
                dst.unlink()
            shutil.copy2(src, dst)
            # Archives carry commit timestamps, which can predate cached objects.
            # Only changed inputs need a fresh mtime for make dependency checks.
            os.utime(dst, None)
    return current


def dependency_source_digest():
    digest = hashlib.sha256()
    for name in source_files(SUPPORT / 'deps'):
        path = SUPPORT / 'deps' / name
        digest.update(name.encode() + b'\0')
        data = os.readlink(path).encode() if path.is_symlink() else path.read_bytes()
        digest.update(hashlib.sha256(data).digest())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--source-commit', required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    metadata_path = out / 'build.json'
    metadata = json.loads(metadata_path.read_text())
    if 'copied_source_files' not in metadata:
        parser.error('this older work directory has no source manifest; use a clean build once')
    if (metadata['dependencies'] != json.loads((SUPPORT / 'dependencies.json').read_text()) or
            metadata.get('dependency_source_sha256') != dependency_source_digest()):
        parser.error('declared dependency sources changed; use a clean build')
    git = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=ROOT,
                         capture_output=True, text=True)
    if git.returncode == 0 and Path(git.stdout.strip()).resolve() == ROOT:
        head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        dirty = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
        if dirty or head != args.source_commit:
            parser.error('source must be the clean committed revision requested')
    base = metadata.get('source_commit')
    metadata['full_kernel_built'] = False
    metadata.pop('kernel_sha256', None)
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')
    metadata['copied_source_files'] = sync_source(ROOT, out / 'work/xnu', metadata['copied_source_files'])
    if metadata.get('source_sync_version') != 2:
        # One-time recovery for work trees produced by the old timestamp copier.
        for name in metadata['copied_source_files']:
            path = out / 'work/xnu' / name
            if not path.is_symlink():
                os.utime(path, None)
    metadata['source_sync_version'] = 2
    metadata.update(source_commit=args.source_commit, incremental_base_commit=base)
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')
    from uart_console import stage_console
    metadata['uart_console'] = stage_console(out / 'work/xnu', out / 'work/generated-tools')
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')
    subprocess.run(['bash', str(SUPPORT / 'driver.sh'), str(out / 'work')], check=True)
    kernel = out / 'work/fakeroot/System/Library/Kernels/kernel.development.a04br3'
    shutil.copyfile(kernel, out / kernel.name)
    metadata.update(full_kernel_built=True, kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest())
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()
