/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "A04BR3Ethernet.h"
#include "A04BR3Apple80211SET.h"
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/kpi_mbuf.h>
#include <net/if.h>
OSDefineMetaClassAndStructors(A04BR3EthernetInterface,IOEthernetInterface);
OSDefineMetaClassAndStructors(A04BR3EthernetController,IOEthernetController);

bool A04BR3EthernetInterface::initBackend(IONetworkController *controller,const a04_network_backend *b)
{
    if(!b || !b->state)return false;
    backend=*b;
    if(!IOEthernetInterface::init(controller))return false;
    return setInterfaceSubType(IFNET_SUBFAMILY_WIFI);
}

IOReturn A04BR3EthernetInterface::attachToDataLinkLayer(IOOptionBits,void *)
{
    /* Use the documented virtual extension point. Do NOT call Apple's default
     * attach here: it would allocate another ifnet and a different softc.
     * IONetworkStack owns name/unit assignment and registered-state transition. */
    int error=network.attach(this,&backend,getUnitNumber());
    if(error){IOLog("A04BR3 network: genuine interface DLIL attach errno=%d\n",error);return kIOReturnError;}
    return kIOReturnSuccess;
}

void A04BR3EthernetInterface::detachFromDataLinkLayer(IOOptionBits,void *)
{
    /* Apple invokes this in sleepable thread-call context. No busy wait and
     * no release of callback context before real DLIL detach completes. */
    int error=network.detachAndWait();
    if(error)IOLog("A04BR3 network: genuine interface detach errno=%d; live bridge retained\n",error);
}

ifnet_t A04BR3EthernetInterface::getIfnet() const { return network.ifnet(); }
UInt16 A04BR3EthernetInterface::getFlags() const
{
    ifnet_t ifp=network.ifnet();
    return ifp?ifnet_flags(ifp):IOEthernetInterface::getFlags();
}
SInt32 A04BR3EthernetInterface::performCommand(IONetworkController *controller,unsigned long command,void *arg0,void *arg1)
{
    if(command==A04_APPLE80211_GET64 || command==A04_APPLE80211_SET64 || command==SIOCSIFFLAGS)
        return network.dispatch(command,arg1);
    return IOEthernetInterface::performCommand(controller,command,arg0,arg1);
}

bool A04BR3EthernetController::configureBackend(const a04_network_backend *b)
{
    if(configured || !b || !b->state)return false;
    a04_network_state state={};
    if(b->state(b->context,&state) || !a04_network_mac_valid(&state))return false;
    backend=*b;configured=true;return true;
}

bool A04BR3EthernetController::start(IOService *provider)
{
    if(!configured || !IOEthernetController::start(provider))return false;
    if(!attachInterface(&interface,true)){
        IOEthernetController::stop(provider);return false;
    }
    return true;
}
void A04BR3EthernetController::stop(IOService *provider)
{
    int error=shutdownBackend();
    if(error){IOLog("A04BR3 network: controller stop deferred errno=%d; backend must remain alive\n",error);return;}
    IOEthernetController::stop(provider);
}
IONetworkInterface *A04BR3EthernetController::createInterface()
{
    auto *child=new A04BR3EthernetInterface;
    if(child && !child->initBackend(this,&backend)){child->release();return nullptr;}
    return child;
}
IOReturn A04BR3EthernetController::getHardwareAddress(IOEthernetAddress *address)
{
    if(!configured || !address)return kIOReturnBadArgument;
    a04_network_state state={};
    if(backend.state(backend.context,&state) || !a04_network_mac_valid(&state))return kIOReturnNotReady;
    memcpy(address->bytes,state.mac,6);return kIOReturnSuccess;
}
IOReturn A04BR3EthernetController::getPacketFilters(UInt32 *filters) const
{
    if(!filters)return kIOReturnBadArgument;
    /* Do not inherit Apple's default advertised multicast/promiscuous support
     * until actual firmware filter programming has a proven backend callback. */
    *filters=0;return kIOReturnSuccess;
}
static IOReturn a04_radio(const a04_network_backend *b,unsigned enable)
{
    a04_network_state state={};
    if(!b->state || b->state(b->context,&state))return kIOReturnNotReady;
    if(enable && !a04_network_control_ready(&state))return kIOReturnNotReady;
    if(!b->set_admin)return kIOReturnUnsupported;
    return b->set_admin(b->context,enable)?kIOReturnError:kIOReturnSuccess;
}
IOReturn A04BR3EthernetController::enable(IONetworkInterface *) { return a04_radio(&backend,1); }
IOReturn A04BR3EthernetController::disable(IONetworkInterface *) { return a04_radio(&backend,0); }
int A04BR3EthernetController::shutdownBackend()
{
    auto *child=OSDynamicCast(A04BR3EthernetInterface,interface);
    if(child){int error=child->shutdownBackend();if(error)return error;}
    if(interface){detachInterface(interface,true);interface=nullptr;}
    return 0;
}
int A04BR3EthernetController::refreshBackend()
{
    auto *child=OSDynamicCast(A04BR3EthernetInterface,interface);
    return child?child->refreshBackend():ENXIO;
}
int A04BR3EthernetController::receive(mbuf_t m)
{
    auto *child=OSDynamicCast(A04BR3EthernetInterface,interface);
    if(!child){if(m)mbuf_freem(m);return ENXIO;}
    return child->receive(m);
}
