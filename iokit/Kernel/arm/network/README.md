# A04BR3 BSD network adapter

This original component creates a real Ethernet-family DLIL ifnet with WiFi
subfamily and the hardware MAC supplied by a real backend. It is not a private
IO80211Interface implementation. No built-in scan records, association success,
MAC address, firmware-ready defaults, or Apple user client are provided.
`tests/` alone supplies mock backend state/results.

## Owner integration

Add `iokit/Kernel/arm/network/A04BR3BSDNetwork.cpp optional iokitcpp` to the
kernel's files.arm64 in the integrating change. Existing configuration and SDIO
sources are intentionally outside this component's edit scope. The real WLAN
owner creates and attaches an IOService child under the SDIO provider, then calls
`attach(child, &backend, unit)` after BSD Ethernet plumbing is available. Use an
available unit; attach errors must be handled, not treated as ready. The bridge
retains the child. Do not advertise a private IO80211 class or instantiate a fake
IONetworkingFamily user client. State, callback context and the bridge object
must remain alive until detach completion. Backend callbacks run under the
bridge lock and must not reenter it; transmit must queue promptly.

`firmware_running` refers to actual WLAN main firmware, not WMT download ACKs.
`control_ready` is the real WLAN command transport. `set_admin(1)` enables radio
control before scan and association. IFF_UP requires control readiness only,
never association or transmit support. Native GET likewise requires observed
control readiness, independently of admin UP and association. A successful
radio command is not a connection result. State is sampled again after that
command. RUNNING is gated by admin UP, real association, real datapath and
transmit callback; it clears on DOWN, disconnect, readiness loss or detach.
XNU defines IFF_RUNNING as resources allocated, so this stronger local policy
must not be mistaken for an independently verified link event or IP reachability.
Call `refresh()` on every real backend state transition. Link event delivery,
RX handoff and private scan/control payload codecs still need integration with
the actual backend; this component alone does not make WiFi usable.

`native_get` receives the observed ARM64 native GET envelope (40-byte ioctl
0xc02869c9, pointer at offset 32), with bounded copyin/copyout scratch. It must
return only real operation results and EOPNOTSUPP for unsupported requests.
No setter or guessed scan opcode is implemented. Maximum payload is 16384;
32-bit and SET envelopes are rejected. No backend result is copied on failure.

On shutdown call `detach()`, wait until `isDetached()`, then `finish()` before
releasing owner/context. Successful detach may complete inline or asynchronously.
A failed detach preserves the live ifnet, restores discovery identity properties,
recomputes readiness from the backend, and leaves admin UP/RUNNING cleared. The
caller can retry detach or explicitly enable UP again. `finish()` returns EBUSY
while a live ifnet remains. Serialize owner attach/refresh/detach/finish calls;
finish must not race an active call. Properties are published only after actual
ifnet attach: IOInterfaceName/Prefix/Unit and role Infrastructure. Native wifid
class matching, parent matching, SC dynamic store delivery and IOServiceOpen
fallback reachability remain unverified on device.

## Validation and sources

Run `python3 tests/test_bsd_network.py`. It compiles the actual C++ bridge against
typed DLIL/IOKit mocks with ASan/UBSan, checks ownership and bounded ioctl copies,
including UP while unassociated, GET before association, no premature RUNNING,
admin failures, state loss, missing TX, inline/async detach and detach failure
recovery. It does not compile against the complete kernel or run native wifid.

SOURCES.json records official Apple API/reference provenance and file hashes.
No Apple reference implementation was copied into this directory. Retained REA
observations and reference downloads are under the worker log directory rather
than this source repository. The native operation backend must implement its
own proven payload codecs; an opaque passthrough is not proof of native binding.
