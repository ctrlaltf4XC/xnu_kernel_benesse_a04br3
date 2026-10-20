# Build sources

The build driver adapts blacktop/darwin-xnu-build at
`663906528d1cd5ea8fde7de6f16de73db9511549` (MIT), originally crediting
https://github.com/pwn0rz/xnu-build. Its license is preserved in
`LICENSE.darwin-xnu-build`. This driver does not download replacement kernel
sources, clone dependencies or apply patches. It builds copies of this tree.

The six dependencies under `deps/` are source snapshots without `.git` from
Apple OSS macOS 13.5. Exact repositories, tags and commits are recorded in
`dependencies.json`; each dependency's original licenses and notices remain.
Changes for installation without source-owner flags and fakeroot header paths
are already incorporated into the committed source.

Kernel source compatibility changes adapt blacktop's kas_info and Ventura
Skywalk changes, and dipa-studio/ios-sweet at
`fdad0de8dc0e75c5ea84762adc03f7232dc91f19`: changes 0001, 0002, 0005,
0007, 0008 and 0009. These cover newer Clang, non-PAC compilation, missing
Apple-only source interfaces, unsupported KPC counters, and unavailable Apple
SoC exports. They are integrated source, not build-time patch files. Original
notices are retained. The unsupported backends do not establish working
interrupts, storage, entropy, graphics or userspace on MT8167.

The Apple Xcode/SDK and KDK are toolchain inputs, not source vendored into this
repository. `sdk.json` declares Xcode 15.4 and KDK 13.5 build 22G74. The script
can install that declared KDK on an explicitly selected macOS build host.
New orchestration code is copyright 2026 yuma shimizu, under `LICENSE.port`.

The interactive debugging workflow uses mxschmitt/action-tmate at
`35b54afac29c97fb54faba5b513f8fbd1882f113`. A command wrapper supplies tmate
with `-a` and the committed public debug key, so access does not depend on
unrelated GitHub account keys. No private key is committed. The workflow keeps
the same runner alive until `/tmp/a04br3-debug-done` is created or timeout.

The A04BR3 early UART alias across TTBR0 teardown adapts the reserved VA mapping approach in dipa-studio/ios-sweet patch `0012-sweet-keep-debug-console-after-ttbr0.patch` (reference snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19). The MT8167 Device mapping and UART tracing are platform-specific adaptations.

Early panic formatting follows ios-sweet patch 0010 (`va_copy` and `_doprnt` before debugger initialization), adapted to the MT8167 UART. In the A04BR3 39-bit layout the retained alias is relative to LOW_GLOBAL_BASE_ADDRESS, not VM_MIN_KERNEL_ADDRESS.

Allocator SplitMix64 RNG and built-in HMAC-SHA256 kernel PRNG registration are adapted from dipa-studio/ios-sweet patches 0013 and 0014 (snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19), scoped to A04BR3. SplitMix64 is used only for non-cryptographic allocator choices; the kernel PRNG uses the built-in HMAC-SHA256 DRBG seeded from the bootloader entropy path. Sebastiano Vigna's public-domain SplitMix64 reference is credited in vm_kern.c.

Absent Image4/AMFI trust-cache service handling adapts ios-sweet patch 0015 (snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19), scoped to A04BR3. Missing services return unavailable/not-found; no trust-cache match or load success is synthesized.

A04BR3PlatformExpert adapts the device-tree platform-expert structure from ios-sweet patch 0016 (snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19), with Benesse compatible/model identity. GICv3 and Kryo CPU details are not imported.
