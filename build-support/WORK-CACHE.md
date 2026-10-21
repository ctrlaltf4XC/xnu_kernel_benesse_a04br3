# Optional work cache

`work_cache.py` never runs or replaces the clean build, incremental build, SDK,
KDK installer or workflow. A miss/rejection leaves the destination absent; the
caller may then use the existing clean recipe. Do not start a build while
exporting. Python must provide `tarfile.data_filter`; there is no unsafe fallback.

From the kernel source root, on the selected Darwin arm64/Xcode15.4 runner:

```sh
python3 build-support/work_cache.py export --out /absolute/original/out/kernel --cache /absolute/new-cache --source-commit "$GITHUB_SHA"
python3 build-support/work_cache.py validate --cache /absolute/new-cache
python3 build-support/work_cache.py restore --out /absolute/original/out/kernel --cache /absolute/new-cache
python3 build-support/incremental.py --out /absolute/original/out/kernel --source-commit "$GITHUB_SHA"
```

If ROOT itself is the real Git top, export requires clean HEAD matching
`build.json.source_commit` (an optional `--source-commit` must also match).
For copied/tarball source without its own Git top, explicit `--source-commit`
(40 lowercase hex digits) is mandatory and must match that metadata. A parent
repository is never adopted; no Git initialization, clone or metadata fabrication
is performed. Both paths compare `source_files(ROOT)` with `copied_source_files`
and verify every current/copied input's bytes, executable mode and symlink target
before compression and before publication. Generated payloads absent from that
source manifest are excluded. A changing source or build rejects publication.
Export/restore probe the actual host, full Xcode build string, selected developer
path and macOS SDK version. Validation checks typed schema1, archive size/SHA256,
SDK/KDK declaration, dependency pins and actual dependency-source digest, build
recipe digests, metadata source/include list and archived Xcode build-request
path/SDK/target-architecture evidence. Kernel source commits may differ at
**restore**: the old commit remains the incremental base until a real rebuild.

The out path must exactly match the recorded absolute path. Generated dependency
files contain that path; relocation is rejected. The destination must **not
exist**, even as an empty directory or dangling symlink. Its canonical parent
must already exist. Extraction stages on that same filesystem and publishes
with no-replace atomic rename (`renamex_np(RENAME_EXCL)` on Darwin; Linux
`renameat2(RENAME_NOREPLACE)` for host tests). A racing existing target is never
replaced. Failed extraction removes only staging. Do not disable validation to
recover a rejected cache.

Only relative regular/dir/symlink/hardlink entries inside archive root `kernel`
are accepted. Parent traversal, absolute links, special/sparse files, duplicate
members, children below links and forward hardlinks are rejected. Every entry
passes the standard data filter plus resolved containment checks; ownership and
unsafe mode bits follow that filter. The same verified open archive descriptor
is retained through extraction. Optional cache contents are executable build
inputs, so use the same trusted provenance as the source/build artifacts; a
SHA256 checksum is not an independent signature.

Restore preserves `work/fakeroot`, generated tools and original source/include
metadata, but sets `full_kernel_built=false`, deletes `kernel_sha256`, records
`cache_restored_from`, and removes a top-level cached kernel artifact if present.
Only the existing incremental recipe may produce a new kernel and set successful
build metadata. Restoring fakeroot does **not** prove a fresh runner can skip
installing/downloading KDK; that SDK experiment remains outstanding.

## Recovered 9ac cache

For `runner-cache-compact-9ac0704/{manifest.json,kernel-work.tar.xz}`:

```sh
python3 build-support/work_cache.py validate --legacy --cache /path/to/runner-cache-compact-9ac0704
python3 build-support/work_cache.py restore --legacy --cache /path/to/runner-cache-compact-9ac0704 --out /Users/runner/work/xnu_kernel_benesse_a04br3/xnu_kernel_benesse_a04br3/out/kernel
```

`--legacy` read-only normalizes **only** source commit
`9ac070452dd3745fdb704b81beaabc5dbc181768`. It uses the archive's actual
`kernel/build.json` and libfirehose `build-request.json`, the retained fixed
runner/out/developer context, and recipe hashes from that exact source revision.
It does not modify the old manifest. Darwin/host-arm64 in this legacy context is
the retained runner fact, not something inferred solely from target `ARCHS`.
Actual restore still requires the host probe to match. `validate` is an offline
structural/compatibility check and explicitly reports `host_restore_tested=false`.
The real archive contains 18,877 regular files, 2,308 dirs, 65 symlinks and 40
hardlinks; read-only validation has passed. Actual Darwin restore and subsequent
SDK build are separate parent-run checks, not established by this tool's host
unit tests.

Host regression (tiny synthetic caches; no private cache or SDK dependency):

```sh
PYTHONPATH=build-support:build-support/tests python3 -m unittest test_work_cache
```

The workflow can start SSH before its first build with
`defer_initial_build=true` and `keep_ssh=true`. Upload the optional cache,
restore into the absent `out/kernel`, then run the incremental command above
from the fixed copied source revision. If cache validation fails, leave the
target absent and use the normal `build.py --install-kdk` clean build instead.
The default workflow still builds from clean source without any private cache.
