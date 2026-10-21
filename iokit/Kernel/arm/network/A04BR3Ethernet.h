/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original A04 integration of pinned genuine Apple IONetworkingFamily classes.
 * No private IO80211 class/vtable identity or link result is impersonated.
 */
#ifndef A04BR3_ETHERNET_H
#define A04BR3_ETHERNET_H
#include "apple/IOEthernetController.h"
#include "apple/IOEthernetInterface.h"
#include "A04BR3BSDNetwork.h"
class A04BR3EthernetInterface : public IOEthernetInterface {
    OSDeclareDefaultStructors(A04BR3EthernetInterface);
    A04BR3BSDNetwork network;
    a04_network_backend backend;
public:
    bool initBackend(IONetworkController *,const a04_network_backend *);
    IOReturn attachToDataLinkLayer(IOOptionBits,void *) APPLE_KEXT_OVERRIDE;
    void detachFromDataLinkLayer(IOOptionBits,void *) APPLE_KEXT_OVERRIDE;
    ifnet_t getIfnet() const APPLE_KEXT_OVERRIDE;
    UInt16 getFlags() const APPLE_KEXT_OVERRIDE;
    SInt32 performCommand(IONetworkController *,unsigned long,void *,void *) APPLE_KEXT_OVERRIDE;
    int shutdownBackend(){ return network.detachAndWait(); }
    int refreshBackend(){ return network.refresh(); }
    int receive(mbuf_t m){ return network.input(m); }
};
class A04BR3EthernetController : public IOEthernetController {
    OSDeclareDefaultStructors(A04BR3EthernetController);
    a04_network_backend backend;
    IONetworkInterface *interface=nullptr;
    bool configured=false;
public:
    /* Owner sets callbacks before start; MAC comes from verified runtime NVRAM.
     * No files, user configuration, extracted asset or per-user build needed. */
    bool configureBackend(const a04_network_backend *);
    bool start(IOService *) APPLE_KEXT_OVERRIDE;
    void stop(IOService *) APPLE_KEXT_OVERRIDE;
    IONetworkInterface *createInterface() APPLE_KEXT_OVERRIDE;
    IOReturn getHardwareAddress(IOEthernetAddress *) APPLE_KEXT_OVERRIDE;
    IOReturn getPacketFilters(UInt32 *) const APPLE_KEXT_OVERRIDE;
    IOReturn enable(IONetworkInterface *) APPLE_KEXT_OVERRIDE;
    IOReturn disable(IONetworkInterface *) APPLE_KEXT_OVERRIDE;
    int shutdownBackend();
    int refreshBackend();
    int receive(mbuf_t); /* consumes real DLIL-formatted mbuf, including failures */
};
#endif
