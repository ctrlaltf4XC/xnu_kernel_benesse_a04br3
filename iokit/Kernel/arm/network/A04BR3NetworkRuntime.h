/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_NETWORK_RUNTIME_H
#define A04BR3_NETWORK_RUNTIME_H
#include "A04BR3Ethernet.h"
/* Called once from the board's serialized startup work context after runtime
 * NVRAM validation and BSD initialization. No per-user extraction/config/build.
 * Provider/backend context live for the boot, including asynchronous callbacks.
 * The real Apple IONetworkStack service remains a boot-lifetime singleton.
 * On error no controller is returned; errno is set. On success caller owns the
 * initial controller reference. Use stop before detach/release during teardown. */
A04BR3EthernetController *a04br3_network_start(IOService *provider,
    const a04_network_backend *backend,int *error);
#endif
