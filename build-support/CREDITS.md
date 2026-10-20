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

A04BR3CPU adapts ios-sweet patch 0018 boot-CPU registration (snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19). GICv3 SGI code is replaced with MT8167 GICv2 MMIO. Timer PPI 27 and GICD/GICC ranges come from the A04BR3 stock DTS and existing mouseos/edk2-mtk A04BR3 implementation. GICv2 handling is original board code; secure Group 0 remains under ATF.

Late inherited-watchdog quiescence follows mouseos/edk2-mtk Platform/Benesse/A04BR3Pkg/Drivers/OsHandoffDxe/OsHandoffDxe.c: TOPRGU MODE key 0x22000000 and enable-bit clear, preserving other mode bits. It runs only after boot-CPU IRQ validation, not in the early shim.

The built-in pthread provider copies Apple libpthread-514.100.2 at a6c70ca0e544347a0377babf748f831577ec24f9 (kern/private/include, with upstream license headers), paired with xnu-8796.141.3. Integration wrappers and open-header compatibility adaptations derive from dipa-studio/ios-sweet patch 0019 and its staging script (snapshot fdad0de8dc0e75c5ea84762adc03f7232dc91f19). All provider sources are committed; no build-time source fetch or clone is required. TargetConditionals.h is obtained only from the declared installed Xcode SDK by the checked-in driver.

The A04BR3 legacy AES CBC/ECB provider adapts ios-sweet patch 0020 and copies the portable BearSSL constant-time bitslice AES at 7bea48e5e850ab4cafbe68d3765cdaba13a86d6f, downloaded from the official Gitweb snapshot. Selected source files, headers, upstream metadata and MIT LICENSE.txt are committed under libkern/crypto/a04br3_bearssl. No runtime or build-time source fetch, SIMD or AES CPU extension is required for this provider.
