# A04BR3 build runner debugging

The manual Actions workflow copies this repository's selected revision without Git history, starts Upterm 0.38.0 with `debug.pub` as its allowed client key, and builds using Xcode 15.4 and the declared KDK. Upterm is an external terminal relay (https://github.com/owenthereal/upterm); it is a debugging dependency, not a kernel dependency.

The `a04br3/ssh` commit status contains the SSH command. Use the private key corresponding to `debug.pub`; no private key is included in the repository. For another maintainer, replace the public key with their own before dispatching. Never upload a private key.

The runner retains the workspace after a failed build. From its working directory, inspect `out/build.log`. To retry incrementally after changing the copied sources under `out/kernel/work/xnu`, run:

```sh
bash source/build-support/driver.sh "$PWD/out/kernel/work" > out/retry.log 2>&1
```

Record every source correction in this repository and rerun a clean build before claiming reproducibility. An incremental build alone is diagnostic evidence. Existing exported headers under `out/kernel/work/fakeroot` may need regeneration when a header or its export list changes.

The expected installed kernel is `out/kernel/work/fakeroot/System/Library/Kernels/kernel.development.a04br3`. The clean Python builder produces the kernel copy and `build.json` only after the driver succeeds and that file exists. Do not infer success from an SSH connection or header generation.

End the retained runner with `touch /tmp/a04br3-debug-done`. The job also has a 120-minute limit. Transfer needed logs and outputs before ending it. The current diagnostic workflow avoids artifact upload because account artifact storage was full; that does not affect compilation.
