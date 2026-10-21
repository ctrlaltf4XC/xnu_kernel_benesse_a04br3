/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Real DLIL interface, using XNU's existing Ethernet family/plumber. Original
 * implementation using the APIs referenced in SOURCES.json; no vendor binary
 * ABI or private IO80211 class is copied/emulated here.
 */
#include "A04BR3BSDNetwork.h"
#include "A04BR3Apple80211IOCTL.h"
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/kpi_mbuf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/ethernet.h>

static int a04_network_copyin(void *,uint64_t address,void *out,unsigned bytes)
{return copyin((user_addr_t)address,out,bytes);}
static int a04_network_copyout(void *,const void *in,uint64_t address,unsigned bytes)
{return copyout(in,(user_addr_t)address,bytes);}

A04BR3BSDNetwork::A04BR3BSDNetwork():interface(nullptr),gate(nullptr),registry(nullptr),
    backend{},stopping(false),admin(false),mac{},name{} {}

int A04BR3BSDNetwork::snapshot(a04_network_state *s)
{
    *s={};
    if(stopping || !backend.state)return ENXIO;
    int error=backend.state(backend.context,s);
    if(error)return error;
    if(!a04_network_mac_valid(s) || memcmp(mac,s->mac,6))return ENXIO;
    return 0;
}

int A04BR3BSDNetwork::attach(IOService *service,const a04_network_backend *b,unsigned unit)
{
    if(gate || !service || !service->getProvider() || !b || !b->state || unit>999999)return EINVAL;
    a04_network_state state={};int error=b->state(b->context,&state);
    if(error)return error;
    if(!a04_network_mac_valid(&state))return ENXIO;
    gate=IOLockAlloc();if(!gate)return ENOMEM;
    backend=*b;memcpy(mac,state.mac,6);registry=service;registry->retain();
    struct ifnet_init_eparams init={};
    init.ver=IFNET_INIT_CURRENT_VERSION;init.len=sizeof(init);init.flags=IFNET_INIT_LEGACY;
    init.uniqueid=mac;init.uniqueid_len=6;init.name="en";init.unit=unit;
    init.family=IFNET_FAMILY_ETHERNET;init.type=IFT_ETHER;init.subfamily=IFNET_SUBFAMILY_WIFI;
    init.output=output;init.demux=ether_demux;init.add_proto=ether_add_proto;init.del_proto=ether_del_proto;
    init.check_multi=ether_check_multi;init.framer_extended=ether_frameout_extended;
    init.softc=this;init.ioctl=ioctl;init.detach=detached;
    static const uint8_t broadcast[6]={255,255,255,255,255,255};init.broadcast_addr=broadcast;init.broadcast_len=6;
    error=ifnet_allocate_extended(&init,&interface);
    if(error){interface=nullptr;finish();return error;}
    snprintf(name,sizeof(name),"%s%u",ifnet_name(interface),ifnet_unit(interface));
    error=ifnet_set_mtu(interface,1500);
    if(!error)error=ifnet_set_flags(interface,IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST,
        IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST|IFF_UP|IFF_RUNNING);
    struct sockaddr_dl address={};address.sdl_len=sizeof(address);address.sdl_family=AF_LINK;
    address.sdl_type=IFT_ETHER;address.sdl_alen=6;memcpy(LLADDR(&address),mac,6);
    if(!error)error=ifnet_attach(interface,&address);
    if(error){ifnet_release(interface);interface=nullptr;finish();return error;}
    /* Publish only after the real BSD interface exists, never claim connected. */
    registry->setProperty("IOInterfaceName",name);
    registry->setProperty("IOInterfaceNamePrefix",ifnet_name(interface));
    registry->setProperty("IOInterfaceUnit",(uint64_t)ifnet_unit(interface),32);
    registry->setProperty("IO80211InterfaceRole","Infrastructure");
    registry->setProperty("WLANBackendReady",false);
    registry->setProperty("WLANDataPathReady",false);
    registry->registerService();
    int status=refresh();
    if(status)IOLog("A04BR3 network: interface attached, backend error=%d; not ready\n",status);
    return 0;
}

int A04BR3BSDNetwork::refresh()
{
    if(!gate)return ENXIO;
    IOLockLock(gate);
    if(!interface || stopping){IOLockUnlock(gate);return ENXIO;}
    a04_network_state s={};int error=snapshot(&s);
    bool control=!error && a04_network_control_ready(&s) && backend.native_get;
    bool data=control && a04_network_data_ready(&s) && backend.transmit;
    ifnet_set_flags(interface,(admin && data)?IFF_RUNNING:0,IFF_RUNNING);
    registry->setProperty("WLANBackendReady",control);
    registry->setProperty("WLANDataPathReady",data);
    IOLockUnlock(gate);return error;
}

errno_t A04BR3BSDNetwork::output(ifnet_t ifp,mbuf_t packet)
{
    auto *s=(A04BR3BSDNetwork *)ifnet_softc(ifp);
    if(!s || !s->gate){mbuf_freem(packet);return ENXIO;}
    IOLockLock(s->gate);a04_network_state state={};int error=s->snapshot(&state);
    if(!error && (!s->admin || !a04_network_data_ready(&state)))error=ENETDOWN;
    if(!error)error=s->backend.transmit?s->backend.transmit(s->backend.context,packet):EOPNOTSUPP;
    IOLockUnlock(s->gate);
    if(error)mbuf_freem(packet); /* only successful backend TX owns the packet */
    return error;
}

errno_t A04BR3BSDNetwork::ioctl(ifnet_t ifp,unsigned long command,void *data)
{
    auto *s=(A04BR3BSDNetwork *)ifnet_softc(ifp);if(!s || !s->gate)return ENXIO;
    if(command!=A04_APPLE80211_GET64 && command!=SIOCSIFFLAGS)return EOPNOTSUPP;
    IOLockLock(s->gate);a04_network_state state={};int error=s->snapshot(&state);
    if(command==SIOCSIFFLAGS){
        bool enabled=(ifnet_flags(ifp)&IFF_UP)!=0;
        /* UP enables radio/control before scan/association. It must not depend
         * on association or a usable packet datapath (that would deadlock setup).
         * RUNNING remains derived from observed association + datapath only. */
        if(!error && enabled && !a04_network_control_ready(&state))error=ENETDOWN;
        if(!error)error=s->backend.set_admin?s->backend.set_admin(s->backend.context,enabled):EOPNOTSUPP;
        if(!error){
            s->admin=enabled;
            /* Radio control can change backend state synchronously. Refresh
             * the snapshot rather than promoting pre-command state to link. */
            a04_network_state after={};int status=s->snapshot(&after);
            bool control=!status && a04_network_control_ready(&after) && s->backend.native_get;
            bool linked=control && a04_network_data_ready(&after) && s->backend.transmit;
            ifnet_set_flags(ifp,(enabled && linked)?IFF_RUNNING:0,IFF_RUNNING);
            s->registry->setProperty("WLANBackendReady",control);
            s->registry->setProperty("WLANDataPathReady",linked);
        }
        else{s->admin=false;ifnet_set_flags(ifp,0,IFF_UP|IFF_RUNNING);}
    }else if(!error){
        if(!data)error=EINVAL;
        else{
            auto *req=(a04_apple80211_req64 *)data;
            if(!req->length || req->length>A04_APPLE80211_MAX_BYTES)error=EINVAL;
            else{
                unsigned bytes=req->length;void *scratch=IOMallocZero(bytes);
                if(!scratch)error=ENOMEM;
                else{
                    const a04_network_copy copy={nullptr,a04_network_copyin,a04_network_copyout};
                    error=a04_network_get(&s->backend,&state,req,s->name,scratch,bytes,&copy);
                    IOFree(scratch,bytes);
                }
            }
        }
    }
    IOLockUnlock(s->gate);return error;
}

int A04BR3BSDNetwork::detach()
{
    if(!gate)return 0;
    IOLockLock(gate);if(!interface || stopping){IOLockUnlock(gate);return 0;}
    stopping=true;admin=false;auto ifp=interface;
    ifnet_set_flags(ifp,0,IFF_UP|IFF_RUNNING);
    registry->removeProperty("IOInterfaceName");registry->removeProperty("IOInterfaceNamePrefix");
    registry->removeProperty("IOInterfaceUnit");registry->removeProperty("IO80211InterfaceRole");
    registry->setProperty("WLANBackendReady",false);registry->setProperty("WLANDataPathReady",false);
    IOLockUnlock(gate);
    int error=ifnet_detach(ifp); /* may invoke detached() inline */
    if(error){
        /* DLIL still owns a live interface. Restore discovery identity, but
         * retain admin-down state; a new explicit UP is required to enable it. */
        IOLockLock(gate);stopping=false;
        registry->setProperty("IOInterfaceName",name);
        registry->setProperty("IOInterfaceNamePrefix",ifnet_name(interface));
        registry->setProperty("IOInterfaceUnit",(uint64_t)ifnet_unit(interface),32);
        registry->setProperty("IO80211InterfaceRole","Infrastructure");
        IOLockUnlock(gate);
        (void)refresh();
    }
    return error;
}

void A04BR3BSDNetwork::detached(ifnet_t ifp)
{
    auto *s=(A04BR3BSDNetwork *)ifnet_softc(ifp);
    ifnet_release(ifp); /* release the original allocation reference, once */
    IOLockLock(s->gate);s->interface=nullptr;IOLockUnlock(s->gate);
}

bool A04BR3BSDNetwork::isDetached()
{
    if(!gate)return true;
    IOLockLock(gate);bool done=interface==nullptr;IOLockUnlock(gate);return done;
}

int A04BR3BSDNetwork::finish()
{
    if(!isDetached())return EBUSY;
    if(registry){registry->release();registry=nullptr;}
    if(gate){IOLockFree(gate);gate=nullptr;}
    backend={};stopping=admin=false;return 0;
}
