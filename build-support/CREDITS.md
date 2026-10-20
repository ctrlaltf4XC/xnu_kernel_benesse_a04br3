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

Root bring-up follows ios-sweet patches 0021, 0022, and 0024 at
fdad0de8dc0e75c5ea84762adc03f7232dc91f19: Apple mockfs enabled,
node allocation type corrected, virtual memdev I/O and 4KiB page units
corrected, and absent IOStorageFamily classes handled on A04BR3 only.

The same reference's patches 0023 and 0025 supply built-in SHA-256 for
code-signing hashes and an AMFI interface reporting no entitlements.
Page hashes remain validated; entitlement queries grant no permissions.

Built-in HFS copies Apple hfs-627.100.6 at
31d5ec238dfab485e78a9a30b8b92d13e8914f03 (core and hfs_encodings,
original notices and APPLE_LICENSE retained). Integration follows ios-sweet
0026 at fdad0de8dc0e75c5ea84762adc03f7232dc91f19. The source is committed;
the declared SDK supplies TargetConditionals.h through the build driver.

The HFS volume UUID path uses MD5. Its legacy provider follows ios-sweet
0027 and includes BearSSL md5/dec32le/enc32le at the already pinned commit,
with the same preserved MIT license. It is not used for security decisions.

The initial read-only MSDC0 probe adapts mouseos EDK2
Mt8167Pkg/Drivers/EmmcBlockIoDxe/EmmcBlockIoDxe.c and A04BR3
SdCardBlockIoDxe register definitions (BSD-2-Clause-Patent). It retains LK
clock/timing/card state, sends CMD13/CMD8/CMD17, and restores host CFG/INTEN.

MSDC source references: https://github.com/mouseos/edk2-mt8167 at
f45890c754d5a4955971cb0b2ccbeed5f2a842ee and
https://github.com/mouseos/edk2-mtk at
1982d888c765f5b12f6e0dce71f1ed8bc92ac50b. Hardware addresses are MT8167
specific; the reference phone's Qualcomm UFS driver is not used.

Unsealed HFS root bring-up follows the problem documented by ios-sweet
0029. A04BR3 requires the explicit a04br3.unsealed_root=1 boot argument and
only permits an HFS root reporting an unsupported root-authentication ioctl;
other authentication errors still fail. This is an unsigned development port.

Portable kernel SHA-1 and SHA-384/512 adapters follow ios-sweet patches
0032/0033 at fdad0de8dc0e75c5ea84762adc03f7232dc91f19. BearSSL hash and
big-endian codec sources use the existing pinned revision and MIT license.

XML entitlement parsing and development platform-binary policy adapt
ios-sweet 0035/0036 at fdad0de8dc0e75c5ea84762adc03f7232dc91f19. XML must
match the code directory; DER-only signatures grant no entitlements.
A04BR3 additionally requires a04br3.platform_root=1 and a read-only root
for development platform status; this is not production signature trust.

Writable HFS root development mode follows ios-sweet 0031 (same reference
revision above), gated by both a04br3.root_rw=1 and a04br3.emmc_rw=1.
MT8167 PIO write command/register fields were cross-checked against the
local MT8167 Linux driver (eb47f42e1f84fe5716ef239620725f82718a620a,
drivers/mmc/host/mtk-sd.c); no Linux driver code is copied. CMD24 writes
are limited to GPT userdata, with CMD13 programming completion and optional
EXT_CSD FLUSH_CACHE. Whole-disk and raw character writes remain denied.
After a writable boot, an old generated HFS image is no longer a valid
delta baseline. Restore a complete generated image for the next root change.

Development-only AppleKeyStore IOService follows ios-sweet 0038 at the
reference revision above. It requires a04br3.dev_keystore=1, provides an
always-unlocked software test handle and rejects unsupported selectors.
It implements no SEP, passcode protection, encryption or cryptographic key
wrapping and must not be described as data protection.

Board bindfs enablement and default-off user core dumps follow ios-sweet
patches 0037 and 0039 at fdad0de8dc0e75c5ea84762adc03f7232dc91f19.
The native Apple bindfs source is already present; the board configuration
now includes it for launchd firmware directory mounts. kern.coredump can
still explicitly enable user core files.

Bounded native-process register snapshots draw on ios-sweet 0054 at the
reference revision above, rewritten to copy at most 16 register records
per task, without user memory reads or printing while holding task locks.
Three snapshots at 120-second intervals are enabled only for full-root bring-up.

Development IOWatchdog is adapted from ios-sweet 0040 and 0045 at the
reference revision. It is gated by a04br3.full_root=1, acknowledges known
user watchdog calls and PMU boot-stage properties, and never arms a timer.
It is not an implementation of the MT8167 watchdog or an Apple PMU. Unknown
selectors and oversized outputs are rejected.
