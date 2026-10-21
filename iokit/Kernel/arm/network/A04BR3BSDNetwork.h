/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_BSD_NETWORK_H
#define A04BR3_BSD_NETWORK_H
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <net/kpi_interface.h>
#include "A04BR3NetworkBackend.h"
/* Owner supplies an attached real IOService child with a provider parent.
 * Object and backend context must live until isDetached() after detach().
 * This class is not an IO80211Interface/IONetworkingFamily vtable substitute.
 */
class A04BR3BSDNetwork {
public:
    A04BR3BSDNetwork();
    A04BR3BSDNetwork(const A04BR3BSDNetwork &)=delete;
    A04BR3BSDNetwork &operator=(const A04BR3BSDNetwork &)=delete;
    int attach(IOService *registry,const a04_network_backend *backend,unsigned unit);
    int refresh();
    int detach();
    bool isDetached();
    int finish(); /* releases retained registry/gate only after detach callback */
private:
    ifnet_t interface;
    IOLock *gate;
    IOService *registry;
    a04_network_backend backend;
    bool stopping,admin;
    uint8_t mac[6];
    char name[16];
    int snapshot(a04_network_state *);
    static errno_t output(ifnet_t,mbuf_t);
    static errno_t ioctl(ifnet_t,unsigned long,void *);
    static void detached(ifnet_t);
};
#endif
