# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute real adapter/bridge code with typed Apple/DLIL lifecycle mocks.
Full pinned family SDK compilation is a separate integration gate.
"""
from pathlib import Path
import hashlib,json,subprocess,tempfile,unittest
from test_bsd_network import STUB,CASE,SOURCE
APPLE=r'''
#pragma once
using UInt16=uint16_t;using UInt32=uint32_t;using SInt32=int32_t;using IOOptionBits=unsigned;using IOReturn=int;
#define TRUE true
#define APPLE_KEXT_OVERRIDE override
#define OSDeclareDefaultStructors(x)
#define OSDefineMetaClassAndStructors(x,y)
#define OSDynamicCast(t,p) dynamic_cast<t *>(p)
static const int kIOReturnSuccess=0,kIOReturnError=1,kIOReturnBadArgument=2,kIOReturnNotReady=3,kIOReturnUnsupported=4,kIOReturnBusy=5;
static const unsigned kConfigFrozen=1,kConfigTxPull=2,kConfigRxPoll=4,kConfigDataRates=8;
struct IOEthernetAddress{uint8_t bytes[6];};
struct OSSymbol {};
static OSSymbol mockNetworkGroup,mockOtherGroup;
static const OSSymbol *gIONetworkFilterGroup=&mockNetworkGroup;
static const UInt32 kIOPacketFilterUnicast=1,kIOPacketFilterBroadcast=2,kIOPacketFilterMulticast=16,kIOPacketFilterPromiscuous=256;
class IONetworkController:public IOService {};
struct ifnet_stat_increment_param {unsigned packets_in,bytes_in,errors_in,collisions,packets_out,bytes_out,errors_out;};
struct if_bandwidths_t {uint64_t max_bw,eff_bw;};
struct IONetworkLinkEventData {uint64_t linkSpeed;};
static const unsigned kIONetworkEventTypeLinkUp=1,kIONetworkEventTypeLinkDown=2,kIONetworkEventTypeLinkSpeedChange=3,kIONetworkEventTypeDLIL=4,kIONetworkEventWakeOnLANSupportChanged=5;
static const unsigned KEV_VENDOR_APPLE=1,KEV_NETWORK_CLASS=1,KEV_DL_SUBCLASS=1,KEV_DL_LINK_ON=1,KEV_DL_LINK_OFF=2,IFNAMSIZ=16;
using thread_call_param_t=void *;
static int baseInputs;
static int ifnet_input_extended(ifnet_t p,mbuf_t a,mbuf_t b,const ifnet_stat_increment_param *stats){assert(p && p==current && a==b && stats->packets_in==1);lastBaseIfnet=p;baseInputs++;mbuf_freem(a);return 0;}
static unsigned ifnet_family(ifnet_t p){return p->init.family;}
static int ifnet_set_bandwidths(ifnet_t p,if_bandwidths_t *,if_bandwidths_t *){assert(p);return 0;}
static bool thread_call_enter1(void *,thread_call_param_t){return true;}
class IONetworkInterface:public IOService {
protected:
 ifnet_t _backingIfnet=nullptr;IOLock *_privateLock=nullptr;unsigned _configFlags=0;UInt16 _flags=0;
 struct Stats {unsigned inputErrors=0,collisions=0;} _driverStats,_lastDriverStats;
 ifnet_stat_increment_param _inputDeltas={};void *_inputEventThreadCall=nullptr;
 IOReturn adoptExistingIfnet(ifnet_t);
 IOReturn clearExistingIfnet(ifnet_t);
public:
 virtual ~IONetworkInterface(){FREE_BACKING_FRAGMENT if(_privateLock)IOLockFree(_privateLock);}
 void pushInputPacket(mbuf_t,uint32_t);
 bool inputEvent(UInt32,void *);
 virtual bool init(IONetworkController *){_privateLock=IOLockAlloc();return IOService::init();}
 virtual IOReturn attachToDataLinkLayer(IOOptionBits,void *){assert(false);return 1;}
 virtual void detachFromDataLinkLayer(IOOptionBits,void *){assert(false);}
 virtual ifnet_t getIfnet() const;
 virtual UInt16 getFlags() const;
 virtual bool setFlags(UInt16,UInt16=0);
 virtual SInt32 performCommand(IONetworkController *,unsigned long,void *,void *){return EOPNOTSUPP;}
 virtual bool setInterfaceSubType(unsigned v){assert(v==IFNET_SUBFAMILY_WIFI);return true;}
 virtual UInt16 getUnitNumber() const{return 7;}
};
class IOEthernetInterface:public IONetworkInterface {};
static IONetworkInterface *mockAttachedInterface;
class IOEthernetController:public IONetworkController {
public:
 virtual bool start(IOService *){return true;}
 virtual void stop(IOService *){}
 virtual IONetworkInterface *createInterface()=0;
 virtual IOReturn getHardwareAddress(IOEthernetAddress *)=0;
 virtual IOReturn getPacketFilters(UInt32 *) const{return 1;}
 virtual IOReturn enablePacketFilter(const OSSymbol *,UInt32,UInt32,IOOptionBits){return kIOReturnUnsupported;}
 virtual IOReturn enable(IONetworkInterface *){return 1;}
 virtual IOReturn disable(IONetworkInterface *){return 1;}
 virtual bool attachInterface(IONetworkInterface **out,bool){
  auto *p=createInterface();if(!p)return false;
  if(!p->attach(this)){p->release();return false;}
  if(p->attachToDataLinkLayer(0,nullptr)){p->detach(this);p->release();return false;}
  mockAttachedInterface=p;*out=p;return true;
 }
 virtual void detachInterface(IONetworkInterface *p,bool){p->detachFromDataLinkLayer(0,nullptr);p->detach(this);p->release();}
};
class IONetworkStack:public IOService {
public:bool start(IOService *){return true;}
};
'''
EXTRA=r'''
class BackingProbe:public IOEthernetInterface {
public:
 IOReturn take(ifnet_t p){return adoptExistingIfnet(p);}
 void mode(unsigned v){_configFlags=v;}
};
static void seamFallbackCase(const a04_network_backend &b){
 reset();A04BR3BSDNetwork bridge;IOService service;
 assert(!bridge.attach(&service,&b,7));auto *probe=new BackingProbe;
 assert(probe->init(nullptr));assert(probe->take(nullptr)==kIOReturnBadArgument);
 probe->mode(kConfigRxPoll);assert(probe->take(current)==kIOReturnUnsupported && current->refs==1);
 probe->mode(0);assert(!probe->take(current) && current->refs==2);
 assert(probe->take(current)==kIOReturnBusy && current->refs==2);
 assert(!bridge.detachAndWait() && current && current->refs==1 && probe->getIfnet()==current);
 probe->release(); // execute genuine base free() backing-reference fragment
 assert(!current && releaseCount==1 && !lockCount && !childCount);
}
static void bindingCases(){
 reset();a04_network_backend b={nullptr,state,get,admin,tx,set};IOService provider;
 realState.firmware_running=realState.control_ready=1;
 int error=99;auto *controller=a04br3_network_start(&provider,&b,&error);
 assert(controller && !error && current && childCount==3);
 assert(dynamic_cast<IO80211Controller *>(controller));
 assert(dynamic_cast<IO80211Interface *>(mockAttachedInterface));
 assert(current->refs==2); // one allocated ifnet: bridge ref + genuine base ref
 auto *actual=OSDynamicCast(A04BR3EthernetInterface,mockAttachedInterface);assert(actual);
 assert(actual->getIfnet()==current); // actual Apple getter sees private field
 actual->retain();
 Packet basePacket={false};actual->pushInputPacket(&basePacket,64);
 assert(basePacket.freed && baseInputs==1 && lastBaseIfnet==current);
 kern_event_msg event={};assert(actual->inputEvent(kIONetworkEventTypeDLIL,&event) && baseEvents==1 && lastBaseIfnet==current);
 actual->setFlags(IFF_UP|IFF_RUNNING);assert(!(current->flags&(IFF_UP|IFF_RUNNING)));
 IOEthernetAddress mac;assert(controller->getHardwareAddress(&mac)==0 && !memcmp(mac.bytes,realState.mac,6));
 assert(controller->enable(nullptr)==0 && lastAdmin==1 && !realState.associated);
 UInt32 filters=999;assert(!controller->getPacketFilters(&filters) && filters==(kIOPacketFilterUnicast|kIOPacketFilterBroadcast));
 assert(controller->enablePacketFilter(gIONetworkFilterGroup,kIOPacketFilterUnicast,0,0)==0);
 assert(controller->enablePacketFilter(gIONetworkFilterGroup,kIOPacketFilterBroadcast,0,0)==0);
 assert(controller->enablePacketFilter(gIONetworkFilterGroup,kIOPacketFilterMulticast,0,0)==kIOReturnUnsupported);
 assert(controller->enablePacketFilter(gIONetworkFilterGroup,kIOPacketFilterPromiscuous,0,0)==kIOReturnUnsupported);
 assert(controller->enablePacketFilter(&mockOtherGroup,kIOPacketFilterUnicast,0,0)==kIOReturnUnsupported);
 realState.control_ready=0;assert(controller->enablePacketFilter(gIONetworkFilterGroup,kIOPacketFilterUnicast,0,0)==kIOReturnNotReady);realState.control_ready=1;
 assert(controller->refreshBackend()==0 && !(current->flags&IFF_RUNNING));
 assert(!controller->scanCompleted() && scanEvents==1 && current->refs==2 && !realState.associated);
 realState.control_ready=0;assert(controller->scanCompleted()==ENETDOWN && scanEvents==1);realState.control_ready=1;
 eventError=EIO;assert(controller->scanCompleted()==EIO && current->refs==2);eventError=0;
 auto *child=OSDynamicCast(A04BR3EthernetInterface,controller->createInterface());assert(child);child->release();
 auto r=req();assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&r));
 assert(!realState.associated && !(current->flags&IFF_RUNNING));
 failDetach=EIO;assert(controller->shutdownBackend()==EIO && current->refs==2 && actual->getIfnet()==current);failDetach=0;
 controller->stop(&provider);assert(!actual->getIfnet() && !current && releaseCount==1);
 actual->release(); // genuine base fallback cannot release an already-cleared ref
 controller->detach(&provider);controller->release();
 // Apple stack is boot-lifetime in production; test explicitly ends the boot.
 a04Stack->detach(&provider);a04Stack->release();a04Stack=nullptr;
 assert(!current && !childCount && !lockCount);
 for(unsigned fault=0;fault<2;fault++){
  reset();realState.firmware_running=realState.control_ready=1;
  A04BR3EthernetController failed;
  assert(failed.configureBackend(&b));if(fault==0)failReference=EIO;else failAttach=EIO;
  assert(!failed.start(&provider) && !current && !childCount && !lockCount);failReference=0;
 }
 reset();realState.mac_valid=0;assert(!a04br3_network_start(&provider,&b,&error) && error==ENXIO && !childCount && !current);
 seamFallbackCase(b);
}
'''
NATIVE_API=r'''
#pragma once
#include "A04BR3WLANBackendAPI.h"
static a04_wlan_native_client nativeClient={};
static unsigned observedCount,observedDone,observedGeneration=1,acquires,releases,beginCalls,beginActive;
static int runtimeError,beginError,copyError;
static a04_wlan_scan_record scanRecords[2];
static void recordInit(unsigned i){
 auto &r=scanRecords[i];r={};auto &a=r.ap;auto &x=r.observation;
 a.mac[0]=2;a.mac[5]=(uint8_t)(i+1);memcpy(a.ssid,"abc",3);a.ssid_length=3;
 a.rate_count=2;a.rates[0]=0x82;a.rates[1]=0x84;a.channel=6;a.band=1;a.capability=0x431;a.beacon=100;a.rcpi_valid=1;a.rcpi=100;
 x.generation=observedGeneration;x.observed_us=1000000;x.from_probe=i;
 const uint8_t ies[]={0,3,'a','b','c',1,2,0x82,0x84};x.ie_bytes=sizeof(ies);memcpy(x.ies,ies,sizeof(ies));
}
extern "C" int a04br3_wlan_acquire_backend(IOService *p,a04_network_backend *b){
 if(!realState.control_ready)return ENXIO;
 p->retain();acquires++;*b={nullptr,state,get,admin,tx,set};return 0;
}
extern "C" void a04br3_wlan_release_backend(IOService *p){releases++;p->release();}
extern "C" int a04br3_wlan_scan_snapshot(IOService *,a04_wlan_scan_info *out){
 *out={};out->count=observedCount;out->complete=observedDone;out->generation=observedGeneration;out->error=runtimeError;out->mode=beginActive;return 0;
}
extern "C" int a04br3_wlan_scan_copy_record(IOService *,unsigned generation,unsigned i,a04_wlan_scan_record *out){
 if(copyError)return copyError;
 if(generation!=observedGeneration || !observedDone)return EAGAIN;
 if(i>=observedCount)return ENOENT;*out=scanRecords[i];return 0;
}
extern "C" int a04br3_wlan_scan_mode(IOService *,unsigned active,unsigned *accepted){
 beginCalls++;beginActive=active;if(beginError)return beginError;
 *accepted=++observedGeneration;observedDone=observedCount=0;return 0;
}
extern "C" int a04br3_wlan_bind_native(IOService *,const a04_wlan_native_client *c){nativeClient=*c;c->owner->retain();return 0;}
extern "C" int a04br3_wlan_unbind_native(IOService *){nativeClient.owner->release();nativeClient={};return 0;}
#define MBUF_DONTWAIT 0
static int mbuf_allocpacket(int,unsigned,void *,mbuf_t *){return ENOMEM;}
static int mbuf_copyback(mbuf_t,unsigned,unsigned,const void *,int){assert(false);return EIO;}
static unsigned mbuf_len(mbuf_t){return 0;}
static void mbuf_pkthdr_setheader(mbuf_t,void *){}
static void *mbuf_data(mbuf_t){return nullptr;}
static void mbuf_adj(mbuf_t,unsigned){}
'''
NATIVE_CASE=r'''
static void nativeCases(){
 reset();IOService provider;IOService *owner=nullptr;
 realState.firmware_running=realState.control_ready=1;
 assert(!a04br3_network_bind(&provider,&owner) && owner && !current);
 auto client=nativeClient;client.ready(client.context,&provider);
 assert(current && current->refs==2 && provider.refs==2);
 uint8_t policy[5456]={},out[1164]={};uint32_t n=sizeof(out);
 policy[0]=1;policy[4]=3;policy[0x34]=3;policy[0x38]=1;out[0]=1;out[0x8b]=4;
 observedDone=observedCount=0;client.changed(client.context);assert(!scanEvents);
 assert(client.set(client.context,10,0,policy,sizeof(policy))==EAGAIN && !scanEvents);
 observedCount=2;client.changed(client.context);assert(!scanEvents); // AP observation is not completion
 observedDone=1;client.changed(client.context);assert(scanEvents==1);
 recordInit(0);recordInit(1);
 assert(!client.set(client.context,10,0,policy,sizeof(policy)));
 for(unsigned i=0;i<2;i++){
  memset(out,0,sizeof(out));out[0]=1;out[0x8b]=4;n=sizeof(out);
  assert(!client.get(client.context,11,0,out,&n) && out[0x21]==i+1 && out[0x22]==0 && a04_scan_result_u32(out+0x84)==1234);
  assert(out[0x8a]==9 && !memcmp(out+0x8c,scanRecords[i].observation.ies,9));
 }
 memset(out,0,sizeof(out));out[0]=1;out[0x8b]=4;n=sizeof(out);
 assert(client.get(client.context,11,0,out,&n)==EIO);
 // Actual provider adapter rejects oversized full IEs off-stack, no truncation.
 assert(!client.set(client.context,10,0,policy,sizeof(policy)));
 scanRecords[0].observation.ie_bytes=1025;int oldAllocations=allocations;
 uint8_t saved[1164];memcpy(saved,out,sizeof(out));
 assert(client.get(client.context,11,0,out,&n)==EMSGSIZE && allocations==oldAllocations && !memcmp(saved,out,sizeof(out)));
 scanRecords[0].observation.ie_bytes=9;copyError=EAGAIN;
 assert(client.get(client.context,11,0,out,&n)==EAGAIN && allocations==oldAllocations);copyError=0;
 runtimeError=83;assert(client.get(client.context,11,0,out,&n)==EIO && allocations==oldAllocations);runtimeError=0;
 client.changed(client.context);assert(scanEvents==1);
 observedDone=0;client.changed(client.context);assert(scanEvents==1);
 observedGeneration++;observedCount=0;observedDone=1;client.changed(client.context);assert(scanEvents==2);
 assert(!client.set(client.context,10,0,policy,sizeof(policy)) && scanEvents==2);
 for(unsigned i=0;i<3;i++){assert(client.get(client.context,11,0,out,&n)==EIO && n==sizeof(out) && scanEvents==2);}
 policy[0x34]=1;beginError=EOPNOTSUPP;
 assert(client.set(client.context,10,0,policy,sizeof(policy))==EOPNOTSUPP && scanEvents==2 && beginActive==1);
 beginError=0;assert(!client.set(client.context,10,0,policy,sizeof(policy)) && !observedDone && beginActive==1);
 memset(out,0,sizeof(out));out[0]=1;out[0x8b]=4;n=sizeof(out);
 assert(client.get(client.context,11,0,out,&n)==EAGAIN && scanEvents==2);
 policy[0x34]=2;assert(!client.set(client.context,10,0,policy,sizeof(policy)) && beginActive==0 && scanEvents==2);
 assert(client.set(client.context,99,0,nullptr,0)==EOPNOTSUPP);
 observedDone=0;client.changed(client.context);observedDone=1;realState.control_ready=0;
 client.changed(client.context);assert(scanEvents==2); // poisoned/offline backend must not emit completion
 realState.control_ready=1;client.changed(client.context);assert(scanEvents==3);
 assert(!a04br3_network_unbind(&provider,&owner) && !owner && !current && provider.refs==1);
 assert(acquires==releases);
 a04Stack->detach(&provider);a04Stack->release();a04Stack=nullptr;
 assert(!childCount && !lockCount);
}
'''
class EthernetBinding(unittest.TestCase):
 def test_adapter(self):
  stub=STUB.replace('static int childCount,failChildInit','static int failReference;\nstatic int childCount,failChildInit').replace('assert(p&&p->refs);p->refs++;return 0;','assert(p&&p->refs);if(failReference)return failReference;p->refs++;return 0;').replace('class IOService {','class IOService {\npublic: virtual ~IOService()=default;').replace('public: virtual ~IOService()=default;', 'public: virtual ~IOService()=default; virtual void free(){}').replace('childCount--;delete this;', 'childCount--;free();delete this;').replace('bool init(){heap=true;','bool init(void * =nullptr){heap=true;')
  case=CASE.replace(' traceCases();\n a04_network_backend b=', ' a04_network_backend b=').replace('#include "driver.cpp"','#include "driver.cpp"\n#include "apple/IOEthernetController.h"\n#include "A04BR3Ethernet.cpp"\n#include "A04BR3NetworkRuntime.cpp"').replace('int main(){',EXTRA+'\n#include \"sdio/mock_provider.h\"\n#include \"A04BR3NativeRendezvous.cpp\"\n'+NATIVE_CASE+'\nint main(){traceCases();nativeCases();bindingCases();')
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'stub.h').write_text(stub)
   for name in ['IOKit/IOLib.h','IOKit/IOService.h','net/kpi_interface.h','sys/systm.h','sys/sockio.h','sys/kpi_mbuf.h','sys/kauth.h','sys/kern_event.h','libkern/OSAtomic.h','kern/clock.h','kern/thread.h','net/if.h','net/if_dl.h','net/if_types.h','net/if_ether.h','net/ethernet.h']:
    h=p/name;h.parent.mkdir(parents=True,exist_ok=True);h.write_text('#include "stub.h"\n')
   (p/'sdio').mkdir();(p/'sdio/mock_provider.h').write_text(NATIVE_API)
   sdio=SOURCE.parent/'sdio'
   (p/'sdio/A04BR3WLANBackendAPI.h').write_text((sdio/'A04BR3WLANBackendAPI.h').read_text().replace('../network/','../'))
   security=(sdio/'A04BR3WLANSecurity.h').read_text()
   mic=next(line for line in security.splitlines() if line.startswith('typedef int (*a04_wlan_mic_verify)'))
   (p/'sdio/A04BR3WLANData.h').write_text('#pragma once\n'+mic+'\n')
   station=(sdio/'A04BR3WLANStation.h').read_text();a=station.index('struct a04_wlan_ap {');b=station.index('};',a)+2
   (p/'sdio/A04BR3WLANStation.h').write_text('#pragma once\n'+station[a:b])
   (p/'sdio/A04BR3WLANScanSnapshot.h').write_text((sdio/'A04BR3WLANScanSnapshot.h').read_text())
   (p/'A04BR3NativeRendezvous.cpp').write_text((SOURCE/'A04BR3NativeRendezvous.cpp').read_text().replace('../sdio/','sdio/'))
   (p/'apple').mkdir()
   original=(SOURCE/'apple/IONetworkInterface.cpp').read_text()
   def function(signature):
    start=original.index(signature);at=original.index('{',start);depth=1;end=at+1
    while depth:
     if original[end]=='{':depth+=1
     elif original[end]=='}':depth-=1
     end+=1
    return original[start:end]+'\n'
   macros=original[original.index('#define IO_IFNET_GET'):original.index('//------------------------------------------------------------------------------',original.index('#define IO_IFNET_GET'))]
   real=macros+'\nIO_IFNET_GET(getFlags, UInt16, flags, ifnet_flags)\nIO_IFNET_RMW(setFlags, UInt16, flags, ifnet_set_flags)\n'
   real+=function('ifnet_t IONetworkInterface::getIfnet')
   real+='#define ABS(a) ((a)<0?-(a):(a))\n'+function('void IONetworkInterface::pushInputPacket')+function('bool IONetworkInterface::inputEvent')
   real+=(SOURCE/'apple/A04BR3IONetworkBacking.inc').read_text()
   freeStart=original.index('    if (_backingIfnet)',original.index('void IONetworkInterface::free'))
   freeEnd=original.index('\n    }',freeStart)+6
   apple=APPLE.replace('FREE_BACKING_FRAGMENT',original[freeStart:freeEnd])
   (p/'apple/base.h').write_text('#include "stub.h"\n'+apple+'\n'+real)
   for name in ['IOEthernetController.h','IOEthernetInterface.h','IONetworkStack.h']:(p/'apple'/name).write_text('#include "base.h"\n')
   for h in SOURCE.glob('*.h'):(p/h.name).write_text(h.read_text().replace('../sdio/','sdio/'))
   for name in ['A04BR3Ethernet.cpp','A04BR3NetworkRuntime.cpp']:(p/name).write_text((SOURCE/name).read_text())
   (p/'driver.cpp').write_text((SOURCE/'A04BR3BSDNetwork.cpp').read_text());(p/'case.cpp').write_text(case)
   for words in (0,1):
    with self.subTest(event_header_bytes=24+4*words):
     subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DA04_TEST_EVENT_WORDS='+str(words),'-I',str(p),str(p/'case.cpp'),'-o',str(p/'case')],check=True)
     subprocess.run([str(p/'case')],check=True)
 def test_source_wireless_registration(self):
  # Genuine source-defined metaclasses, one functional implementation each;
  # not IOClass properties or guessed private Apple vtable declarations.
  impl=(SOURCE/'A04BR3Ethernet.cpp').read_text()
  header=(SOURCE/'A04BR3Ethernet.h').read_text()
  for child,parent in [('IO80211Controller','IOEthernetController'),('IO80211Interface','IOEthernetInterface')]:
   self.assertIn('class '+child+' : public '+parent,header)
   declaration='OSDefineMetaClassAndStructors('+child+','+parent+')'
   self.assertIn(declaration,impl)
   definitions=[]
   for f in SOURCE.parents[3].rglob('*.cpp'):
    if declaration in f.read_text(errors='replace'):definitions.append(f)
   self.assertEqual(definitions,[SOURCE/'A04BR3Ethernet.cpp'])
  self.assertIn('return network.dispatch(command,arg1)',impl)
  self.assertNotIn('setProperty("IOClass"',impl)
  self.assertIn('if(type!=10)return EOPNOTSUPP;',(SOURCE/'A04BR3NativeRendezvous.cpp').read_text())
  files=(SOURCE.parents[2]/'conf/files.arm64').read_text()
  self.assertEqual(files.count('network/A04BR3Ethernet.cpp'),1)
  self.assertNotIn('IO80211Family',files)
 def test_pinned_provenance(self):
  manifest=json.loads((SOURCE/'apple/SOURCE.json').read_text())
  self.assertEqual(manifest['revision'],'bb910e2b458939f6b1f53c0fa6907f258b431ea4')
  for f in manifest['files']:
   self.assertEqual(hashlib.sha256((SOURCE/'apple'/f['path']).read_bytes()).hexdigest(),f['vendored_sha256'])
if __name__=='__main__':unittest.main()
