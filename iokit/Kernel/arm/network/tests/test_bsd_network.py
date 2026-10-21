# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Compile actual bridge source with typed DLIL/IOKit mocks and ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile,unittest
SOURCE=Path(__file__).resolve().parents[1]

STUB=r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <map>
#include <string>
#include <stdarg.h>
using errno_t=int;using user_addr_t=uint64_t;using protocol_family_t=uint32_t;
struct Packet {bool freed;};using mbuf_t=Packet *;
struct IF;using ifnet_t=IF *;
using Out=int (*)(ifnet_t,mbuf_t);using Ioctl=int (*)(ifnet_t,unsigned long,void *);using Detached=void (*)(ifnet_t);
struct IOLock {bool held;};
static int lockCount,allocations,registrations,releaseCount,packetFrees,txCalls,nativeCalls,copyIns,copyOuts;
static int failAlloc,failMtu,failFlags,failAttach,failDetach,failMemory,copyInError,copyOutError;
static int sleeps,wakes;
#define THREAD_UNINT 0
static int childCount,failChildInit,failChildAttach,inputCalls,inputError,heldLocks;
static bool inlineDetach;
static int credentialRoot=1,credentialChecks;
using kauth_cred_t=int;
static kauth_cred_t kauth_cred_get(){credentialChecks++;return credentialRoot;}
static int kauth_cred_issuser(kauth_cred_t c){return c;}
static IOLock *IOLockAlloc(){lockCount++;return new IOLock{false};}
static void IOLockLock(IOLock *p){assert(p&&!p->held);p->held=true;heldLocks++;}
static void IOLockUnlock(IOLock *p){assert(p&&p->held);p->held=false;heldLocks--;}
static void IOLockFree(IOLock *p){assert(p&&!p->held);delete p;lockCount--;}
static int IOLockSleep(IOLock *,void *,int);
static void IOLockWakeup(IOLock *p,void *,bool){assert(p->held);wakes++;}
static void *IOMallocZero(unsigned n){if(failMemory)return nullptr;allocations++;return calloc(1,n);}
static void IOFree(void *p,unsigned){assert(p&&allocations>0);allocations--;free(p);}
static void IOLog(const char *,...) {}
class IOService {
public:
 bool parent=true,heap=false;IOService *provider=nullptr;unsigned refs=1;std::map<std::string,std::string> text;std::map<std::string,uint64_t> number;
 IOService *getProvider(){return parent?(provider?provider:this):nullptr;}
 bool init(){heap=true;childCount++;return !failChildInit;}
 bool attach(IOService *p){if(failChildAttach)return false;provider=p;return true;}
 void detach(IOService *p){assert(p==provider);provider=nullptr;parent=false;}
 void retain(){refs++;}void release(){assert(refs);refs--;if(!refs){assert(heap);childCount--;delete this;}}
 void setProperty(const char *k,const char *v){text[k]=v;}
 void setProperty(const char *k,uint64_t v,int){number[k]=v;}
 void setProperty(const char *k,bool v){number[k]=v;}
 void setProperty(const char *k,void *p,unsigned n){text[k]=std::string((char *)p,n);}
 void removeProperty(const char *k){text.erase(k);number.erase(k);}
 void registerService(){registrations++;}
};
#define IFNET_INIT_CURRENT_VERSION 2
#define IFNET_INIT_LEGACY 1
#define IFNET_FAMILY_ETHERNET 2
#define IFNET_SUBFAMILY_WIFI 3
#define IFT_ETHER 6
#define AF_LINK 18
#define IFF_UP 1
#define IFF_RUNNING 64
#define IFF_BROADCAST 2
#define IFF_SIMPLEX 2048
#define IFF_MULTICAST 32768
#define SIOCSIFFLAGS 0x80206910UL
struct sockaddr {};
struct sockaddr_dl {uint8_t sdl_len,sdl_family;uint16_t sdl_index;uint8_t sdl_type,sdl_nlen,sdl_alen,sdl_slen;char sdl_data[24];};
#define LLADDR(p) ((p)->sdl_data+(p)->sdl_nlen)
static int ether_demux(ifnet_t,mbuf_t,char *,protocol_family_t *){return 0;}
struct ifnet_demux_desc {};
static int ether_add_proto(ifnet_t,protocol_family_t,const ifnet_demux_desc *,uint32_t){return 0;}
static int ether_del_proto(ifnet_t,protocol_family_t){return 0;}
static int ether_check_multi(ifnet_t,const sockaddr *){return 0;}
static int ether_frameout_extended(ifnet_t,mbuf_t *,const sockaddr *,const char *,const char *,uint32_t *,uint32_t *){return 0;}
struct ifnet_init_eparams {
 unsigned ver,len,flags;const void *uniqueid;unsigned uniqueid_len;const char *name;unsigned unit,family,type,subfamily;
 Out output;decltype(&ether_demux) demux;decltype(&ether_add_proto) add_proto;decltype(&ether_del_proto) del_proto;
 decltype(&ether_check_multi) check_multi;decltype(&ether_frameout_extended) framer_extended;
 void *softc;Ioctl ioctl;Detached detach;const void *broadcast_addr;unsigned broadcast_len;
};
struct IF {ifnet_init_eparams init;unsigned flags,refs;bool attached;};
static IF *current,*pending;
static int ifnet_allocate_extended(const ifnet_init_eparams *p,ifnet_t *out){
 assert(p->ver==2 && p->len==sizeof(*p) && p->flags==IFNET_INIT_LEGACY);
 assert(!strcmp(p->name,"en") && p->family==IFNET_FAMILY_ETHERNET && p->type==IFT_ETHER && p->subfamily==IFNET_SUBFAMILY_WIFI);
 assert(p->uniqueid_len==6 && !((const uint8_t *)p->uniqueid)[0]);
 if(failAlloc)return ENOMEM;assert(!current);current=new IF{*p,0,1,false};*out=current;return 0;
}
static void *ifnet_softc(ifnet_t p){return p->init.softc;}
static int ifnet_set_mtu(ifnet_t,unsigned n){assert(n==1500);return failMtu;}
static int ifnet_set_flags(ifnet_t p,unsigned v,unsigned mask){p->flags=(p->flags&~mask)|(v&mask);return failFlags;}
static int ifnet_attach(ifnet_t p,const sockaddr_dl *a){assert(a->sdl_alen==6 && a->sdl_family==AF_LINK && !memcmp(LLADDR(a),p->init.uniqueid,6));if(failAttach)return failAttach;p->attached=true;return 0;}
static int ifnet_release(ifnet_t p){assert(p->refs);p->refs--;if(p->refs)return 0;if(p==current)current=nullptr;if(p==pending)pending=nullptr;delete p;releaseCount++;return 0;}
static int ifnet_reference(ifnet_t p){assert(p&&p->refs);p->refs++;return 0;}
static const char *ifnet_name(ifnet_t p){return p->init.name;}
static unsigned ifnet_unit(ifnet_t p){return p->init.unit;}
static unsigned ifnet_flags(ifnet_t p){return p->flags;}
static int ifnet_detach(ifnet_t p){if(failDetach)return failDetach;pending=p;if(inlineDetach)p->init.detach(p);return 0;}
static void completeDetach(){assert(pending);auto p=pending;p->init.detach(p);}
static int IOLockSleep(IOLock *g,void *,int){sleeps++;IOLockUnlock(g);completeDetach();IOLockLock(g);return 0;}
static void mbuf_freem(mbuf_t p){assert(!p->freed);p->freed=true;packetFrees++;}
static int ifnet_input(ifnet_t p,mbuf_t m,const void *){assert(p->refs==2 && !heldLocks);inputCalls++;mbuf_freem(m);return inputError;}
static uint8_t userBuffer[16384];
static int copyin(user_addr_t a,void *out,size_t n){copyIns++;if(copyInError)return copyInError;if(a!=(uintptr_t)userBuffer || n>sizeof(userBuffer))return EFAULT;memcpy(out,userBuffer,n);return 0;}
static int copyout(const void *in,user_addr_t a,size_t n){copyOuts++;if(copyOutError)return copyOutError;if(a!=(uintptr_t)userBuffer || n>sizeof(userBuffer))return EFAULT;memcpy(userBuffer,in,n);return 0;}
'''

CASE=r'''
#include "driver.cpp"
static a04_network_state realState;
static int setCalls,setError;static uint32_t lastSetType,lastSetValue,lastSetBytes;
static int stateError,nativeError,adminError,transmitError,adminCalls;static unsigned backendOutput,lastAdmin;
static int state(void *,a04_network_state *s){*s=realState;return stateError;}
static int get(void *,uint32_t type,uint32_t value,void *p,uint32_t *n){nativeCalls++;assert(type==12 && value==7);if(nativeError)return nativeError;memset(p,0x53,*n);*n=backendOutput;return 0;}
static int set(void *,uint32_t type,uint32_t value,const void *p,uint32_t n){
 setCalls++;lastSetType=type;lastSetValue=value;lastSetBytes=n;
 if(n){assert(p && p!=userBuffer && !memcmp(p,userBuffer,n));}else assert(!p);
 return setError;
}
static int admin(void *,unsigned enabled){adminCalls++;lastAdmin=enabled;return adminError;}
static int tx(void *,void *p){txCalls++;if(transmitError)return transmitError;mbuf_freem((mbuf_t)p);return 0;}
static void reset(){assert(!childCount && !heldLocks);inputCalls=inputError=failChildInit=failChildAttach=0;assert(!lockCount && !allocations && !current && !pending);realState={1,1,0,0,0,0,{0,2,3,4,5,6}};
 stateError=nativeError=adminError=transmitError=failAlloc=failMtu=failFlags=failAttach=failDetach=failMemory=copyInError=copyOutError=0;
 credentialRoot=1;credentialChecks=setCalls=setError=0;adminCalls=0;lastAdmin=99;registrations=releaseCount=packetFrees=txCalls=nativeCalls=copyIns=copyOuts=0;backendOutput=4;inlineDetach=false;}
static a04_apple80211_req64 req(){a04_apple80211_req64 r={};strcpy(r.name,"en7");r.type=12;r.value=7;r.length=8;r.user_data=(uintptr_t)userBuffer;return r;}
static void ready(){realState.firmware_running=realState.control_ready=realState.datapath_ready=realState.associated=1;}
static void cleanup(A04BR3BSDNetwork &n,IOService &s){assert(n.detach()==0);if(pending)completeDetach();assert(n.isDetached() && n.finish()==0 && s.refs==1);assert(!lockCount && !current);}
int main(){
 a04_network_backend b={nullptr,state,get,admin,tx,set};
 // No invented identity or parent; no ifnet allocated for absent hardware.
 for(unsigned fault=0;fault<5;fault++){
  reset();A04BR3BSDNetwork n;IOService s;
  if(fault==0)s.parent=false;if(fault==1)realState.present=0;if(fault==2)realState.mac_valid=0;
  if(fault==3)memset(realState.mac,0,6);if(fault==4)realState.mac[0]=1;
  assert(n.attach(&s,&b,7)!=0 && !current && !lockCount && !registrations && s.refs==1);
 }
 for(unsigned fault=0;fault<4;fault++){
  reset();A04BR3BSDNetwork n;IOService s;
  if(fault==0)failAlloc=1;if(fault==1)failMtu=EIO;if(fault==2)failFlags=EIO;if(fault==3)failAttach=EIO;
  assert(n.attach(&s,&b,7)!=0 && !current && !lockCount && !registrations && s.refs==1);
 }
 reset();A04BR3BSDNetwork n;IOService s;assert(n.attach(&s,&b,7)==0 && current && registrations==1);
 assert(s.text["IOInterfaceName"]=="en7" && s.text["IO80211InterfaceRole"]=="Infrastructure");
 assert(s.text["IOBSDName"]=="en7" && s.number["IOInterfaceType"]==IFT_ETHER && s.text["IOMACAddress"].size()==6);
 assert(!s.number["WLANBackendReady"] && !s.number["WLANDataPathReady"] && !(current->flags&(IFF_UP|IFF_RUNNING)));
 assert(n.finish()==EBUSY);auto r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==ENETDOWN && !copyIns && !nativeCalls);
 Packet p={false};assert(current->init.output(current,&p)==ENETDOWN && p.freed && packetFrees==1 && !txCalls);
 current->flags|=IFF_UP;assert(current->init.ioctl(current,SIOCSIFFLAGS,nullptr)==ENETDOWN && !(current->flags&(IFF_UP|IFF_RUNNING)));
 // Radio UP is permitted before association/datapath. Native control GET
 // reaches only the mock backend; no scan results exist in production code.
 realState.firmware_running=realState.control_ready=1;
 assert(!realState.associated && !realState.datapath_ready);
 current->flags|=IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr));
 assert(adminCalls==1 && lastAdmin==1 && (current->flags&IFF_UP) && !(current->flags&IFF_RUNNING));
 assert(s.number["WLANBackendReady"] && !s.number["WLANDataPathReady"]);
 r=req();assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&r) && r.length==4 && nativeCalls==1);
 // SET credentials are checked before copyin/dispatch; GET remains unprivileged.
 int beforeIn=copyIns,beforeOut=copyOuts;
 credentialRoot=0;r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==EPERM && !setCalls && copyIns==beforeIn);
 assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&r));credentialRoot=1;
 beforeOut=copyOuts;r=req();r.type=0x12345678;r.value=0x9abcdef0;
 assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&r) && setCalls==1);
 assert(lastSetType==0x12345678 && lastSetValue==0x9abcdef0 && lastSetBytes==8 && r.length==8 && copyOuts==beforeOut);
 assert(!(current->flags&IFF_RUNNING)); // SET success never promotes association.
 r=req();r.length=16384;assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&r) && lastSetBytes==16384);
 setError=EIO;r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==EIO);setError=0;
 setError=EOPNOTSUPP;r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==EOPNOTSUPP && r.length==8 && copyOuts==beforeOut);setError=0;
 int beforeSet=setCalls;copyInError=EFAULT;r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==EFAULT && setCalls==beforeSet);copyInError=0;
 for(unsigned fault=0;fault<5;fault++){
  r=req();if(fault==0)memset(r.name,'x',16);if(fault==1)strcpy(r.name,"en8");
  if(fault==2)r.length=16385;if(fault==3)r.user_data=0;if(fault==4)r.user_data=1;
  assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)!=0 && setCalls==beforeSet && copyOuts==beforeOut);
 }
 beforeIn=copyIns;r=req();r.length=0;r.user_data=0;
 assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&r) && lastSetBytes==0 && r.length==0 && copyIns==beforeIn);
 realState.control_ready=0;r=req();beforeSet=setCalls;
 assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==ENETDOWN && setCalls==beforeSet);realState.control_ready=1;
 failMemory=1;r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==ENOMEM && setCalls==beforeSet);failMemory=0;
 // Pure helper also refuses explicit unprivileged use, independently of adapter.
 a04_network_copy c={nullptr,a04_network_copyin,a04_network_copyout};uint8_t scratch[8]={};r=req();
 assert(a04_network_set(&b,&realState,&r,"en7",scratch,8,&c,0)==EPERM && setCalls==beforeSet);
 assert(current->init.ioctl(current,0x802069c8,nullptr)==EOPNOTSUPP);

 p={false};assert(current->init.output(current,&p)==ENETDOWN && p.freed && !txCalls);
 current->flags&=~IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr) && lastAdmin==0 && !(current->flags&IFF_RUNNING));
 adminError=EIO;current->flags|=IFF_UP;assert(current->init.ioctl(current,SIOCSIFFLAGS,nullptr)==EIO && !(current->flags&(IFF_UP|IFF_RUNNING)));adminError=0;
 ready();assert(n.refresh()==0 && s.number["WLANBackendReady"] && s.number["WLANDataPathReady"]);
 assert(!(current->flags&IFF_RUNNING));r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==0 && r.length==4 && userBuffer[0]==0x53);
 current->flags|=IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr));assert(!n.refresh() && (current->flags&IFF_RUNNING));
 p={false};assert(!current->init.output(current,&p) && p.freed && txCalls==1);
 Packet rx={false};assert(!n.input(&rx) && rx.freed && inputCalls==1 && current->refs==1);
 inputError=EIO;rx={false};assert(n.input(&rx)==EIO && rx.freed && inputCalls==2 && current->refs==1);inputError=0;
 transmitError=EIO;p={false};assert(current->init.output(current,&p)==EIO && p.freed && txCalls==2);
 int outputs=copyOuts,calls=nativeCalls;
 for(unsigned fault=0;fault<6;fault++){
  r=req();if(fault==0)memset(r.name,'x',16);if(fault==1)strcpy(r.name,"en8");
  if(fault==2)r.length=0;if(fault==3)r.length=16385;if(fault==4)r.user_data=0;if(fault==5)r.user_data=1;
  assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)!=0 && copyOuts==outputs);
 }
 assert(nativeCalls==calls);
 nativeError=EOPNOTSUPP;r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==EOPNOTSUPP && r.length==8 && copyOuts==outputs);nativeError=0;
 backendOutput=9;r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==EOVERFLOW && copyOuts==outputs);backendOutput=4;
 copyOutError=EFAULT;r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==EFAULT && r.length==8);copyOutError=0;
 assert(current->init.ioctl(current,A04_APPLE80211_SET64,nullptr)==EINVAL);assert(current->init.ioctl(current,0xc02069c9,nullptr)==EOPNOTSUPP);
 realState.datapath_ready=0;assert(!n.refresh() && !(current->flags&IFF_RUNNING) && !s.number["WLANDataPathReady"]);
 realState.firmware_running=0;assert(!n.refresh() && !s.number["WLANBackendReady"]);
 realState.mac[5]++;assert(n.refresh()==ENXIO && !s.number["WLANBackendReady"]);realState.mac[5]--;
 assert(n.detach()==0 && !n.isDetached() && n.finish()==EBUSY && !s.text.count("IOInterfaceName"));
 r=req();assert(current->init.ioctl(current,A04_APPLE80211_GET64,&r)==ENXIO);
 r=req();assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==ENXIO);
 completeDetach();assert(n.isDetached() && n.finish()==0 && releaseCount==1 && s.refs==1);
 reset();A04BR3BSDNetwork fast;IOService t;assert(!fast.attach(&t,&b,7));inlineDetach=true;cleanup(fast,t);
 reset();A04BR3BSDNetwork retry;IOService u;assert(!retry.attach(&u,&b,7));ready();assert(!retry.refresh());current->flags|=IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr));
 failDetach=EIO;assert(retry.detach()==EIO && retry.finish()==EBUSY);
 assert(u.text["IOInterfaceName"]=="en7" && u.text["IO80211InterfaceRole"]=="Infrastructure" && u.number["IOInterfaceUnit"]==7);
 assert(u.number["WLANBackendReady"] && !(current->flags&(IFF_UP|IFF_RUNNING)));
 r=req();assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&r));
 current->flags|=IFF_UP;assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr) && (current->flags&IFF_RUNNING));
 failDetach=0;cleanup(retry,u);
 reset();a04_network_backend missing=b;missing.native_get=nullptr;A04BR3BSDNetwork noControl;IOService v;ready();assert(!noControl.attach(&v,&missing,7) && !v.number["WLANBackendReady"] && !v.number["WLANDataPathReady"]);cleanup(noControl,v);
 reset();a04_network_backend noTx=b;noTx.transmit=nullptr;A04BR3BSDNetwork controlOnly;IOService w;
 realState.firmware_running=realState.control_ready=1;
 assert(!controlOnly.attach(&w,&noTx,7));current->flags|=IFF_UP;
 assert(!current->init.ioctl(current,SIOCSIFFLAGS,nullptr) && lastAdmin==1 && !(current->flags&IFF_RUNNING));
 r=req();assert(!current->init.ioctl(current,A04_APPLE80211_GET64,&r));cleanup(controlOnly,w);
 reset();a04_network_backend noSet=b;noSet.native_set=nullptr;A04BR3BSDNetwork getOnly;IOService z;
 realState.firmware_running=realState.control_ready=1;assert(!getOnly.attach(&z,&noSet,7));r=req();
 assert(current->init.ioctl(current,A04_APPLE80211_SET64,&r)==EOPNOTSUPP && !copyIns && !setCalls);
 cleanup(getOnly,z);
 // Provider-child construction/cleanup and failed construction unwind.
 for(unsigned fault=0;fault<3;fault++){
  reset();A04BR3BSDNetwork failedChild;IOService provider;
  if(fault==0)failChildInit=1;if(fault==1)failChildAttach=1;if(fault==2)realState.present=0;
  assert(failedChild.attachProvider(&provider,&b,7)!=0 && !childCount && !current && !lockCount);
 }
 reset();A04BR3BSDNetwork owned;IOService provider;
 assert(!owned.attachProvider(&provider,&b,7) && childCount==1 && owned.registryEntry()->getProvider()==&provider);
 assert(owned.registryEntry()->text["IOBSDName"]=="en7");
 Packet rxDown={false};assert(owned.input(&rxDown)==ENETDOWN && rxDown.freed && !inputCalls);
 assert(!owned.detach() && owned.finish()==EBUSY && childCount==1);
 Packet rxStopping={false};assert(owned.input(&rxStopping)==ENXIO && rxStopping.freed && !inputCalls);
 assert(!owned.registryEntry()->text.count("IOBSDName") && !owned.registryEntry()->text.count("IOMACAddress"));
 completeDetach();
 assert(!owned.finish() && !childCount && !lockCount);
 Packet rxClosed={false};assert(owned.input(&rxClosed)==ENXIO && rxClosed.freed);
 assert(owned.input(nullptr)==EINVAL);
 reset();A04BR3BSDNetwork wait;IOService parent;
 assert(!wait.attachProvider(&parent,&b,7));int oldSleeps=sleeps;
 assert(!wait.detachAndWait() && sleeps==oldSleeps+1 && !childCount && !lockCount);


}
'''

class BSDNetwork(unittest.TestCase):
    def test_actual_bridge(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'stub.h').write_text(STUB)
            for name in ['IOKit/IOLib.h','IOKit/IOService.h','net/kpi_interface.h','sys/systm.h','sys/sockio.h','sys/kpi_mbuf.h','sys/kauth.h','kern/thread.h','net/if.h','net/if_dl.h','net/if_types.h','net/if_ether.h','net/ethernet.h']:
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "stub.h"\n')
            (root/'driver.cpp').write_text((SOURCE/'A04BR3BSDNetwork.cpp').read_text())
            (root/'case.cpp').write_text(CASE);exe=root/'case'
            subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(root),'-I',str(SOURCE),str(root/'case.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
