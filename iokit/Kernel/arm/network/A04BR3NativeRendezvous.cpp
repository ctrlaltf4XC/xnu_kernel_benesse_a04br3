/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "A04BR3NativeRendezvous.h"
#include "A04BR3NetworkRuntime.h"
#include "A04BR3NativeGET.h"
#include "../sdio/A04BR3WLANBackendAPI.h"
#include <sys/kpi_mbuf.h>
#include <sys/errno.h>
class A04BR3NativeNetwork : public IOService {
    OSDeclareDefaultStructors(A04BR3NativeNetwork);
    A04BR3EthernetController *controller=nullptr;
    IOService *backendProvider=nullptr;
    bool attempted=false;
    static void ready(void *context,IOService *provider){
        auto *self=(A04BR3NativeNetwork *)context;
        if(self->attempted)return;
        self->attempted=true;
        a04_network_backend backend={};
        int error=a04br3_wlan_acquire_backend(provider,&backend);
        if(!error){
            self->backendProvider=provider; // acquire owns one reference
            self->controller=a04br3_network_start(provider,&backend,&error);
            if(error){a04br3_wlan_release_backend(provider);self->backendProvider=nullptr;}
        }
        self->setProperty("NativeNetworkStartError",(uint64_t)error,32);
        IOLog("A04BR3 network: automatic native ready callback start errno=%d controller=%p\n",error,self->controller);
    }
    static void changed(void *context){
        auto *self=(A04BR3NativeNetwork *)context;
        if(self->controller)(void)self->controller->refreshBackend();
    }
    static int get(void *context,uint32_t type,uint32_t value,void *out,uint32_t *bytes){
        auto *self=(A04BR3NativeNetwork *)context;
        if(!self->backendProvider)return ENXIO;
        a04_network_backend backend={};
        int error=a04br3_wlan_acquire_backend(self->backendProvider,&backend);
        if(error)return error;
        a04_network_state state={};error=backend.state(backend.context,&state);
        if(!error)error=a04_native_get(&state,type,value,out,bytes);
        a04br3_wlan_release_backend(self->backendProvider);return error;
    }
    static int set(void *,uint32_t,uint32_t,const void *,uint32_t){return EOPNOTSUPP;}
    static void packet(void *context,unsigned kind,const void *bytes,unsigned length){
        auto *self=(A04BR3NativeNetwork *)context;
        // 1 is real Ethernet delivery in the SDIO-owned API; 2 is EAPOL, which
        // requires a real supplicant and cannot be promoted to an authorized port.
        if(kind!=1 || !self->controller || !bytes || length<14 || length>1514)return;
        mbuf_t m=nullptr;
        if(mbuf_allocpacket(MBUF_DONTWAIT,length,nullptr,&m))return;
        if(mbuf_copyback(m,0,length,bytes,MBUF_DONTWAIT) || mbuf_len(m)<14){mbuf_freem(m);return;}
        mbuf_pkthdr_setheader(m,mbuf_data(m));
        mbuf_adj(m,14); // header remains owned by the mbuf, payload starts at L3
        (void)self->controller->receive(m); // consumes on every return
    }
public:
    int bind(IOService *provider){
        a04_wlan_native_client client={};client.owner=this;client.context=this;
        client.ready=ready;client.changed=changed;client.packet=packet;
        client.get=get;client.set=set; // verify intentionally null: no fake MIC
        return a04br3_wlan_bind_native(provider,&client);
    }
    int shutdown(IOService *provider){
        if(controller){int error=controller->shutdownBackend();if(error)return error;}
        int error=a04br3_wlan_unbind_native(provider);if(error)return error;
        if(controller){controller->stop(provider);controller->detach(provider);controller->release();controller=nullptr;}
        if(backendProvider){a04br3_wlan_release_backend(backendProvider);backendProvider=nullptr;}
        return 0;
    }
};
OSDefineMetaClassAndStructors(A04BR3NativeNetwork,IOService);
extern "C" int a04br3_network_bind(IOService *provider,IOService **owner)
{
    if(!provider || !owner || *owner)return EINVAL;
    auto *native=new A04BR3NativeNetwork;if(!native)return ENOMEM;
    if(!native->init(nullptr)){native->release();return ENOMEM;}
    if(!native->attach(provider)){native->release();return ENXIO;}
    int error=native->bind(provider);
    if(error){native->detach(provider);native->release();return error;}
    *owner=native;return 0;
}
extern "C" int a04br3_network_unbind(IOService *provider,IOService **owner)
{
    if(!provider || !owner)return EINVAL;
    auto *native=OSDynamicCast(A04BR3NativeNetwork,*owner);if(!native)return *owner?EINVAL:0;
    if(native->getProvider()!=provider)return EINVAL;
    int error=native->shutdown(provider);if(error)return error;
    native->detach(provider);native->release();*owner=nullptr;return 0;
}
