# SPDX-License-Identifier: MIT
"""Execute the actual service lifecycle with fake scheduler, locks and bus.
Only ARM MMIO barriers are removed for this host run; no real transport is run.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_wlan_driver_compile import SDIO, write_headers
from test_wlan_runtime_backend import LIVE

STUBS=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct IOLock{bool held;};static unsigned heldLocks,boardReads,readyCalls,changedCalls,readError;
extern "C" int a04br3_network_bind(IOService *,IOService **){assert(!heldLocks);return EOPNOTSUPP;}
static uint64_t mockNow=100;static int registrationError,refs;static void (*networkCallback)();
struct Call{void (*fn)(void *,void *);void *ctx;bool pending;uint64_t deadline;};static Call *scheduled;
extern "C" int a04br3_network_unbind(IOService *s,IOService **owner){assert(!heldLocks && !scheduled);int e=a04br3_wlan_unbind_native(s);*owner=nullptr;return e;}
extern "C" IOLock *IOLockAlloc(){auto *p=new IOLock;p->held=false;return p;}
extern "C" void IOLockFree(IOLock *p){assert(!p->held);delete p;}
extern "C" void IOLockLock(IOLock *p){assert(!p->held);p->held=true;heldLocks++;}
extern "C" void IOLockUnlock(IOLock *p){assert(p->held);p->held=false;heldLocks--;}
extern "C" void *IOMalloc(size_t n){return malloc(n);}extern "C" void IOFree(void *p,size_t){free(p);}
extern "C" void IOSleep(unsigned){}extern "C" void IODelay(unsigned){}
extern "C" void IOLog(const char *,...){}
void OSObject::retain(){refs++;}void OSObject::release(){assert(refs>0);refs--;}
bool IOService::start(IOService *){return true;}void IOService::stop(IOService *){}void IOService::free(){}
bool IOService::setProperty(const char *,OSBoolean *){return true;}bool IOService::setProperty(const char *,uint64_t,unsigned){return true;}
OSObject *IOService::copyProperty(const char *){return nullptr;}void IOService::registerService(){}
OSBoolean yes,no;OSBoolean *kOSBooleanTrue=&yes,*kOSBooleanFalse=&no;
unsigned OSData::getLength(){return 0;}const void *OSData::getBytesNoCopy(){return nullptr;}
PE_state_t PE_state={};extern "C" int PE_parse_boot_argn(const char *,void *,int){return 0;}
extern "C" int SecureDTLookupEntry(DTEntry,const char *,DTEntry *){return 0;}
extern "C" int SecureDTGetProperty(DTEntry,const char *,const void **,unsigned *){return 0;}
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t){assert(false);return 0;}
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t){return 0;}
extern "C" void SHA256_Init(SHA256_CTX *){}extern "C" void SHA256_Update(SHA256_CTX *,const void *,size_t){}extern "C" void SHA256_Final(void *,SHA256_CTX *){}
thread_call_t thread_call_allocate_with_options(void (*fn)(void *,void *),void *ctx,unsigned,unsigned options){assert(options==THREAD_CALL_OPTIONS_ONCE);auto *c=new Call{fn,ctx,false,0};scheduled=c;return c;}
int thread_call_cancel_wait(thread_call_t p){((Call *)p)->pending=false;return 1;}
int thread_call_free(thread_call_t p){assert(!((Call *)p)->pending);delete (Call *)p;scheduled=nullptr;return 1;}
int thread_call_enter_delayed(thread_call_t p,uint64_t deadline){auto *c=(Call *)p;c->deadline=deadline;c->pending=true;return 0;}
void clock_get_system_microtime(clock_sec_t *s,clock_usec_t *u){*s=mockNow/1000000;*u=mockNow%1000000;}
void clock_interval_to_deadline(unsigned n,unsigned scale,uint64_t *out){*out=mockNow+n*scale/1000;}
extern "C" int net_init_add(void (*fn)()){networkCallback=fn;return registrationError;}
extern "C" size_t mbuf_pkthdr_len(mbuf_t){return 0;}
extern "C" int mbuf_copydata(mbuf_t,size_t,size_t,void *){return EIO;}
extern "C" void mbuf_freem(mbuf_t){assert(false);}
extern "C" int a04br3_wlan_read_calibration(void *p,unsigned n){assert(!heldLocks && n==512);boardReads++;if(readError)return readError;board((uint8_t *)p);return 0;}
static void tick(){assert(scheduled && scheduled->pending);auto *c=scheduled;c->pending=false;mockNow=c->deadline;c->fn(c->ctx,nullptr);}
static void ready(void *,IOService *s){assert(!heldLocks);readyCalls++;a04_network_backend backend;assert(!a04br3_wlan_acquire_backend(s,&backend));a04_network_state state;assert(!backend.state(backend.context,&state));assert(state.control_ready && state.radio_enabled && state.mac_valid && !state.associated && !state.datapath_ready);a04br3_wlan_release_backend(s);}
static void changed(void *){assert(!heldLocks);changedCalls++;}
static card c;static a04_sdio_io cio;static a04_sdio_transport ct;static a04_wlan_runtime scratch;
static void prepare(A04BR3SDIO &s){s.transportLock=IOLockAlloc();s.wlanHost=(a04_wlan_host *)IOMalloc(sizeof(*s.wlanHost));init_live(&c,&cio,&ct,s.wlanHost,&scratch);s.wlanDownload.ready=1;s.identityReady=true;}
int main(){
 {A04BR3SDIO s;IOService clientOwner;prepare(s);a04_wlan_native_client client={};client.owner=&clientOwner;client.ready=ready;client.changed=changed;
  assert(!a04br3_wlan_bind_native(&s,&client));s.nativeOwner=&clientOwner;s.deferLive();assert(!boardReads && !scheduled->pending && refs==2);
  a04_network_backend backend;assert(a04br3_wlan_acquire_backend(&s,&backend)==ENXIO);
  networkCallback();assert(boardReads==0 && refs==1 && scheduled->pending);tick();assert(boardReads==1 && captures==1 && s.live->runtime.cap_pending);
  /* One empty poll per invocation: next poll20ms, not8 rounds at100Hz. */
  unsigned reads=c.reads;tick();assert(c.reads-reads<=2 && scheduled->deadline-mockNow==20000);
  capability(&s.live->runtime,0);tick();assert(readyCalls==1 && s.live->runtime.configured && !s.live->runtime.station.sta_active);
  assert(!a04br3_wlan_acquire_backend(&s,&backend));assert(backend.set_admin(backend.context,0)==EOPNOTSUPP);
  unsigned done=0,count=99;assert(!a04br3_wlan_scan_count(&s,&count,&done) && count==0 && !done);
  a04br3_wlan_release_backend(&s);s.stop(nullptr);assert(!refs && !scheduled);s.free();
 }
 /* A callback retained before stop must become harmless after stop. */
 {A04BR3SDIO s;prepare(s);s.deferLive();assert(refs==1);s.stop(nullptr);assert(!refs);networkCallback();assert(!refs && !scheduled);s.free();}
 /* Already initialized takes the same deferred worker, never early I/O. */
 {A04BR3SDIO s;prepare(s);registrationError=EALREADY;readError=EIO;unsigned before=boardReads;s.deferLive();assert(boardReads==before && !refs);tick();assert(boardReads==before+1 && s.live->runtime.failed && !scheduled->pending && !captures);s.stop(nullptr);s.free();}
 assert(!refs && !heldLocks);return 0;
}
'''

class WLANServiceBackend(unittest.TestCase):
    def test_actual_cpp_deferred_boot_callbacks_and_stop(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);write_headers(p)
            # Host compiler cannot emit ARM barrier assembly. Real arm64 test
            # compiles it separately; MMIO routines are never invoked here.
            source=(SDIO/'A04BR3SDIO.cpp').read_text()
            source=source.replace('OSDeclareDefaultStructors(A04BR3SDIO);','OSDeclareDefaultStructors(A04BR3SDIO);\npublic:').replace('private:','public:')
            import re
            source=re.sub(r'__asm__ volatile\("[^"]*":::"memory"\);','',source)
            (p/'IOKit/IOLib.h').write_text((p/'IOKit/IOLib.h').read_text().replace('void bcopy(const void *,void *,size_t);','void bcopy(const void *,void *,size_t) noexcept;').replace('void bzero(void *,size_t);','void bzero(void *,size_t) noexcept;'))
            (p/'test.cpp').write_text('#include <assert.h>\n#include <string.h>\n'+source+'\n'+LIVE+'\n'+STUBS)
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-unused-function',
                            '-fsanitize=address,undefined','-I',str(p),'-I',str(SDIO),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

if __name__=='__main__':unittest.main()
