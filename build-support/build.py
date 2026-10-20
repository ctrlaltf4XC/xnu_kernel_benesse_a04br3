#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the repository's A04BR3 XNU source on macOS/Xcode 15.4."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
from incremental import source_files, dependency_source_digest

SUPPORT = Path(__file__).resolve().parent
ROOT = SUPPORT.parent


def run(args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--install-kdk', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('macOS/Xcode required; use the GitHub workflow')
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('output directory must be empty for a clean source build')
    out.mkdir(parents=True, exist_ok=True)
    sdk = json.loads((SUPPORT / 'sdk.json').read_text())
    version = subprocess.check_output(['xcodebuild', '-version'], text=True)
    if version.splitlines()[0] != 'Xcode ' + sdk['xcode_version']:
        parser.error('select Xcode ' + sdk['xcode_version'] + ' with DEVELOPER_DIR')
    kdk = Path(os.environ.get('KDKROOT', '/Library/Developer/KDKs/KDK_13.5_22G74.kdk'))
    if not kdk.is_dir():
        if not args.install_kdk:
            parser.error('install KDK 13.5 22G74 or use --install-kdk')
        dmg = out / 'KDK.dmg'
        run(['curl', '-fL', '--retry', '3', '--max-time', '900', '-o', dmg, sdk['url']])
        if hashlib.sha256(dmg.read_bytes()).hexdigest() != sdk['sha256']:
            raise RuntimeError('KDK download does not match the declared SDK')
        mount = out / 'kdk-volume'
        mount.mkdir()
        run(['hdiutil', 'attach', '-nobrowse', '-mountpoint', mount, dmg])
        try:
            run(['sudo', '-n', 'installer', '-pkg', mount / 'KernelDebugKit.pkg', '-target', '/'])
        finally:
            run(['hdiutil', 'detach', mount])
    if not kdk.is_dir():
        raise RuntimeError('expected KDK was not installed')
    work = out / 'work'
    work.mkdir()
    shutil.copytree(ROOT, work / 'xnu', symlinks=True,
                    ignore=shutil.ignore_patterns('.git', 'build-support', 'out'))
    dependencies = json.loads((SUPPORT / 'dependencies.json').read_text())
    for entry in dependencies['projects']:
        shutil.copytree(SUPPORT / entry['source_path'], work / entry['name'], symlinks=True)
    metadata = dict(xcode=version, sdk=sdk, dependencies=dependencies,
                    source_commit=os.environ.get('GITHUB_SHA'), full_kernel_built=False,
                    copied_source_files=source_files(ROOT), source_sync_version=2,
                    dependency_source_sha256=dependency_source_digest())
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    env = dict(os.environ, KDKROOT=str(kdk))
    run(['bash', SUPPORT / 'driver.sh', work], env=env)
    kernel = work / 'fakeroot/System/Library/Kernels/kernel.development.a04br3'
    if not kernel.is_file():
        raise RuntimeError('build finished without the requested kernel')
    shutil.copyfile(kernel, out / kernel.name)
    metadata.update(full_kernel_built=True, kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest())
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()
