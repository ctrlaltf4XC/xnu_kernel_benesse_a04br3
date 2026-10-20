# A04BR3 build runner debugging

All kernel and dependency fixes belong in this source repository before a build.
The device repository locks the selected kernel and bootshim commits. No source
correction is stored only inside an Actions runner. A runner can be discarded:
the clean builder reconstructs it from this repository and its declared SDK/KDK.

The workflow copies source without Git history and builds with Xcode 15.4.
The `a04br3/ssh` commit status publishes the Upterm connection command. Only the
client key corresponding to `debug.pub` is accepted. Never upload a private key.
SSH retention is opt-in, bounded to 60 minutes after the build; the whole job
has a 360-minute limit. Keep the same runner through hardware tests and repairs
within that bound. A disconnected guest shell must not end the hosting command.
The workflow uploads the kernel, build.json and build log before retaining SSH.

## Incremental source rebuild

Commit and push a correction locally first. Copy that exact source archive into
a new directory on the same runner (without `.git`), then invoke its builder:

```sh
GITHUB_SHA=COMMIT gh api repos/mouseos/xnu_kernel_benesse_a04br3/tarball/COMMIT > out/repair-source.tar.gz
mkdir out/repair-source
tar -xzf out/repair-source.tar.gz --strip-components=1 -C out/repair-source
python3 out/repair-source/build-support/incremental.py \
  --source-commit COMMIT --out "$PWD/out/kernel" > out/repair.log 2>&1
```

Use a new archive directory for each correction. The script copies changed
source files, removes source files deleted since the previous manifest, keeps
unchanged timestamps, and invokes the checked-in driver. Dependency sources
must still match the prior build declaration. No SDK reinstallation or dependency
rebuild is needed when only kernel source changes. The driver separately tracks
the Cortex-A35 firehose build signature so incompatible archives are rebuilt.
Older work directories without a source manifest require one clean build.

The builder marks metadata incomplete before syncing/building and publishes a
new kernel and successful metadata only after the driver succeeds. Generated
objects are disposable; never make source edits only in `out/kernel/work/xnu`.
An incremental result is distinct from a clean-build reproduction check.

End a retained runner with `touch /tmp/a04br3-debug-done`. Confirm the job has
actually completed. Cancellation, a live runner, a live SSH host, and an active
compiler are different states; an SSH failure alone proves no retention deadline.
