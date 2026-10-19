# XNU for Benesse TAB-A04-BR3

This repository currently contains the unmodified Apple XNU source baseline, not a working MT8167 kernel port. Platform implementation and a complete kernel build remain pending.

## Source and credits

Source: https://github.com/apple-oss-distributions/xnu/tree/xnu-8796.141.3

Tag: `xnu-8796.141.3`  
Commit: `1b191cb58250d0705d8a51287127505aa4bc0789`

The source snapshot was copied without upstream Git history into this independent repository. Apple and the other original contributors retain their copyrights. `APPLE_LICENSE` and per-file notices are preserved; the repository does not relicense upstream source.

The ios-sweet project (https://github.com/dipa-studio/ios-sweet, reference commit `fdad0de8dc0e75c5ea84762adc03f7232dc91f19`) is a porting reference. It is not the source of this XNU snapshot.

Kernel source belongs here. Boot handoff code belongs in `xnu_bootshim_benesse_a04br3`; board configuration and packaging belong in `xnu_device_benesse_a04br3`. Research logs, device dumps and Apple userland binaries are excluded. The shared kernel/board direction leaves both iOS and macOS userland experiments open, without claiming either boots.
