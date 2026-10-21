/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "A04BR3NativeRendezvous.h"
#include "A04BR3NetworkRuntime.h"
#include "A04BR3NativeGET.h"
#include "A04BR3NativePower.h"
#include "A04BR3NativeScanAdapter.h"
#include "A04BR3NativeAssociationAdapter.h"
#include "../sdio/A04BR3WLANBackendAPI.h"
#include <sys/kpi_mbuf.h>
#include <sys/errno.h>
class A04BR3NativeNetwork : public IOService {
    OSDeclareDefaultStructors(A04BR3NativeNetwork);
    A04BR3EthernetController *controller=nullptr;
    IOService *backendProvider=nullptr;
    bool attempted=false;
    IOLock *scanLock=nullptr;
    a04_scan_result_session scanSession={};
    a04_scan_completion_state scanCompletion={};
    a04_scan_backend scanBackend(){return {backendProvider,a04_scan_provider_begin,a04_scan_provider_snapshot};}
    void free() APPLE_KEXT_OVERRIDE {
        if(scanLock){IOLockFree(scanLock);scanLock=nullptr;}
        IOService::free();
    }
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
        if(!self->scanLock || !self->controller)return;
        IOLockLock(self->scanLock);
        auto backend=self->scanBackend();a04_scan_snapshot snapshot={};
        int error=backend.snapshot(backend.context,0,0,&snapshot,nullptr);
        unsigned notify=error?0:a04_scan_completion_reserve(&self->scanCompletion,&snapshot);
        IOLockUnlock(self->scanLock);
        // Reserve under scanLock, release before bridge/DLIL. Concurrent fresh
        // SET returns EBUSY until this exact publication finishes, never waits
        // while holding bridge gate. Older completion cannot satisfy newer SET.
        if(notify){
            error=self->controller->scanCompleted();
            IOLockLock(self->scanLock);
            a04_scan_completion_posted(&self->scanCompletion,snapshot.generation,error);
            IOLockUnlock(self->scanLock);
            if(error)IOLog("A04BR3 network: observed scan completion event errno=%d\n",error);
        }
    }
    static int get(void *context,uint32_t type,uint32_t value,void *out,uint32_t *bytes){
        auto *self=(A04BR3NativeNetwork *)context;
        if(!self->backendProvider)return ENXIO;
        if(type==103)return a04_assoc_provider_current(self->backendProvider,value,out,bytes,a04_scan_provider_now());
        if(type==11){
            if(!self->scanLock)return ENXIO;
            IOLockLock(self->scanLock);auto backend=self->scanBackend();
            int error=a04_scan_dispatch_get(&self->scanSession,&backend,type,value,out,bytes);
            IOLockUnlock(self->scanLock);return error;
        }
        a04_network_backend backend={};
        int error=a04br3_wlan_acquire_backend(self->backendProvider,&backend);
        if(error)return error;
        a04_network_state state={};error=backend.state(backend.context,&state);
        if(!error)error=a04_native_get(&state,type,value,out,bytes);
        a04br3_wlan_release_backend(self->backendProvider);return error;
    }
    static int set(void *context,uint32_t type,uint32_t value,const void *in,uint32_t bytes){
        if(type!=10 && type!=19 && type!=20)return EOPNOTSUPP;
        auto *self=(A04BR3NativeNetwork *)context;
        if(!self->backendProvider)return ENXIO;
        if(type==20)return a04_assoc_provider_set(self->backendProvider,value,in,bytes);
        if(type==19){
            a04_network_backend backend={};
            int error=a04br3_wlan_acquire_backend(self->backendProvider,&backend);
            if(error)return error;
            error=a04_native_power_set(&backend,value,in,bytes);
            a04br3_wlan_release_backend(self->backendProvider);return error;
        }
        if(!self->scanLock)return ENXIO;
        IOLockLock(self->scanLock);auto backend=self->scanBackend();
        int error=a04_scan_dispatch_set(&self->scanSession,&backend,type,value,in,bytes,&self->scanCompletion);
        IOLockUnlock(self->scanLock);return error;
    }
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
        if(!scanLock){scanLock=IOLockAlloc();if(!scanLock)return ENOMEM;}
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
