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
#define APPLE_KEXT_OVERRIDE override
#define OSDeclareDefaultStructors(x)
#define OSDefineMetaClassAndStructors(x,y)
#define OSDynamicCast(t,p) dynamic_cast<t *>(p)
static const int kIOReturnSuccess=0,kIOReturnError=1,kIOReturnBadArgument=2,kIOReturnNotReady=3,kIOReturnUnsupported=4;
struct IOEthernetAddress{uint8_t bytes[6];};
class IONetworkController:public IOService {};
class IONetworkInterface:public IOService {
public:
 virtual bool init(IONetworkController *){return IOService::init();}
 virtual IOReturn attachToDataLinkLayer(IOOptionBits,void *){assert(false);return 1;}
 virtual void detachFromDataLinkLayer(IOOptionBits,void *){assert(false);}
 virtual ifnet_t getIfnet() const{return nullptr;}
 virtual UInt16 getFlags() const{return 0;}
 virtual SInt32 performCommand(IONetworkController *,unsigned long,void *,void *){return EOPNOTSUPP;}
 virtual bool setInterfaceSubType(unsigned v){assert(v==IFNET_SUBFAMILY_WIFI);return true;}
 virtual UInt16 getUnitNumber() const{return 7;}
};
class IOEthernetInterface:public IONetworkInterface {};
class IOEthernetController:public IONetworkController {
public:
 virtual bool start(IOService *){return true;}
 virtual void stop(IOService *){}
 virtual IONetworkInterface *createInterface()=0;
 virtual IOReturn getHardwareAddress(IOEthernetAddress *)=0;
 virtual IOReturn getPacketFilters(UInt32 *) const{return 1;}
 virtual IOReturn enable(IONetworkInterface *){return 1;}
 virtual IOReturn disable(IONetworkInterface *){return 1;}
 virtual bool attachInterface(IONetworkInterface **out,bool){
  auto *p=createInterface();if(!p)return false;
  if(!p->attach(this)){p->release();return false;}
  if(p->attachToDataLinkLayer(0,nullptr)){p->detach(this);p->release();return false;}
  *out=p;return true;
 }
 virtual void detachInterface(IONetworkInterface *p,bool){p->detachFromDataLinkLayer(0,nullptr);p->detach(this);p->release();}
};
class IONetworkStack:public IOService {
public:bool start(IOService *){return true;}
};
'''
EXTRA=r'''
static void bindingCases(){
 reset();a04_network_backend b={nullptr,state,get,admin,tx,set};IOService provider;
 realState.firmware_running=realState.control_ready=1;
 int error=99;auto *controller=a04br3_network_start(&provider,&b,&error);
 assert(controller && !error && current && childCount==3);
 assert(current->refs==1); // only bridge-owned ifnet, no Apple default ifnet
 IOEthernetAddress mac;assert(controller->getHardwareAddress(&mac)==0 && !memcmp(mac.bytes,realState.mac,6));
 assert(controller->enable(nullptr)==0 && lastAdmin==1 && !realState.associated);
 UInt32 filters=999;assert(!controller->getPacketFilters(&filters) && !filters);
 assert(controller->refreshBackend()==0 && !(current->flags&IFF_RUNNING));
 auto *child=OSDynamicCast(A04BR3EthernetInterface,controller->createInterface());assert(child);child->release();
 auto r=req();assert(!current->init.ioctl(current,A04_APPLE80211_SET64,&r));
 assert(!realState.associated && !(current->flags&IFF_RUNNING));
 controller->stop(&provider);controller->detach(&provider);controller->release();
 // Apple stack is boot-lifetime in production; test explicitly ends the boot.
 a04Stack->detach(&provider);a04Stack->release();a04Stack=nullptr;
 assert(!current && !childCount && !lockCount);
 reset();realState.mac_valid=0;assert(!a04br3_network_start(&provider,&b,&error) && error==ENXIO && !childCount && !current);
}
'''
class EthernetBinding(unittest.TestCase):
 def test_adapter(self):
  stub=STUB.replace('class IOService {','class IOService {\npublic: virtual ~IOService()=default;').replace('bool init(){heap=true;','bool init(void * =nullptr){heap=true;')
  case=CASE.replace('#include "driver.cpp"','#include "driver.cpp"\n#include "apple/IOEthernetController.h"\n#include "A04BR3Ethernet.cpp"\n#include "A04BR3NetworkRuntime.cpp"').replace('int main(){',EXTRA+'\nint main(){bindingCases();')
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'stub.h').write_text(stub)
   for name in ['IOKit/IOLib.h','IOKit/IOService.h','net/kpi_interface.h','sys/systm.h','sys/sockio.h','sys/kpi_mbuf.h','sys/kauth.h','kern/thread.h','net/if.h','net/if_dl.h','net/if_types.h','net/if_ether.h','net/ethernet.h']:
    h=p/name;h.parent.mkdir(parents=True,exist_ok=True);h.write_text('#include "stub.h"\n')
   (p/'apple').mkdir();(p/'apple/base.h').write_text('#include "stub.h"\n'+APPLE)
   for name in ['IOEthernetController.h','IOEthernetInterface.h','IONetworkStack.h']:(p/'apple'/name).write_text('#include "base.h"\n')
   for h in SOURCE.glob('*.h'):(p/h.name).write_text(h.read_text())
   for name in ['A04BR3Ethernet.cpp','A04BR3NetworkRuntime.cpp']:(p/name).write_text((SOURCE/name).read_text())
   (p/'driver.cpp').write_text((SOURCE/'A04BR3BSDNetwork.cpp').read_text());(p/'case.cpp').write_text(case)
   subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(p),str(p/'case.cpp'),'-o',str(p/'case')],check=True)
   subprocess.run([str(p/'case')],check=True)
 def test_pinned_provenance(self):
  manifest=json.loads((SOURCE/'apple/SOURCE.json').read_text())
  self.assertEqual(manifest['revision'],'bb910e2b458939f6b1f53c0fa6907f258b431ea4')
  for f in manifest['files']:
   self.assertEqual(hashlib.sha256((SOURCE/'apple'/f['path']).read_bytes()).hexdigest(),f['vendored_sha256'])
if __name__=='__main__':unittest.main()
