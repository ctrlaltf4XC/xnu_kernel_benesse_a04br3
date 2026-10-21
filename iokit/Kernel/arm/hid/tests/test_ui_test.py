# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Execute production private client, native entry points AND event dispatch.
No physical/SDK claim: IOKit primitives are bounded, fault-injectable mocks.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
HID=Path(__file__).resolve().parents[1]
STUB=r'''
#include <stdint.h>
#include <stddef.h>
#ifndef A04_ARM64
#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <pthread.h>
#include <thread>
#include <atomic>
#include <chrono>
#else
extern "C" void abort();
#define assert(x) do {if(!(x))abort();}while(0)
#endif
static int alive,failAt,allocAt,clientAllocFail,initFail,attachFail,startFail,console=1,admin=1;
static int gateDeny,nativeFallback,keysInit,touchInit;
static bool fail(){return failAt && ++allocAt==failAt;}
using UInt32=uint32_t;using UInt64=uint64_t;using IOFixed=int32_t;using IOReturn=int;using IOOptionBits=unsigned;using task_t=void*;
#define APPLE_KEXT_OVERRIDE override
#define OSDeclareDefaultStructors(t)
#define OSDefineMetaClassAndStructors(t,b)
#define OSDynamicCast(t,p) dynamic_cast<t*>(p)
#define OSSafeReleaseNULL(p) do{if(p){(p)->release();(p)=NULL;}}while(0)
#define TRUE 1
#define kOSBooleanTrue true
#define IOLog(...) ((void)0)
enum {kIOReturnSuccess=0,kIOReturnBadArgument=1,kIOReturnUnsupported=2,kIOReturnNoMemory=3,kIOReturnNotPermitted=4,kIOReturnNotReady=5,kIOReturnBusy=6,kIOReturnCannotLock=7};
enum {kHIDPage_Consumer=12,kHIDUsage_Csmr_ConsumerControl=1,kHIDUsage_Csmr_Menu=64,kHIDUsage_Csmr_Power=48,kHIDUsage_Csmr_VolumeIncrement=233,kHIDUsage_Csmr_VolumeDecrement=234,kHIDPage_Digitizer=13,kHIDUsage_Dig_TouchScreen=4};
enum {kIOHIDDigitizerTransducerTypeHand=2,kIOHIDDigitizerTransducerTypeFinger=3};
enum {kIOHIDDigitizerEventFromEdgeTip=1,kIOHIDDigitizerEventTouch=2,kIOHIDDigitizerEventRange=4,kIOHIDDigitizerEventPosition=8,kIOHIDDigitizerEventIdentity=16};
enum {kIOHIDEventFieldDigitizerIndex,kIOHIDEventFieldDigitizerIdentity,kIOHIDEventFieldDigitizerTouch,kIOHIDEventFieldDigitizerEventMask,kIOHIDEventFieldDigitizerCollection,kIOHIDEventFieldDigitizerIsDisplayIntegrated,kIOHIDEventFieldIsBuiltIn,kIOHIDEventFieldDigitizerX,kIOHIDEventFieldDigitizerY,kIOHIDEventFieldDigitizerRange,kIOHIDEventFieldDigitizerButtonMask};
static const int kIOClientPrivilegeAdministrator=1;
static const UInt32 kIOHIDEventServiceUserClientType=0x65737563;
struct OSObject {int refs=1;OSObject(){++alive;}virtual ~OSObject(){--alive;}bool init(){return true;}void retain(){assert(refs>0);++refs;}virtual void free(){}void release(){assert(refs>0);if(!--refs){free();delete this;}}};
struct OSDictionary:OSObject{};
struct OSArray:OSObject {OSObject *data[8]={};unsigned count=0;~OSArray(){for(unsigned i=0;i<count;i++)data[i]->release();}unsigned getCount(){return count;}OSObject *getObject(unsigned i){assert(i<count);return data[i];}bool setObject(OSObject *p){assert(count<8);p->retain();data[count++]=p;return true;}};
struct IOLock {bool held=false;};static IOLock lockPool[32];static unsigned lockNext;
static IOLock *IOLockAlloc(){assert(lockNext<32);return &lockPool[lockNext++];}static void IOLockFree(IOLock*){}
static void IOLockLock(IOLock *l){assert(l&&!l->held);l->held=true;}static void IOLockUnlock(IOLock *l){assert(l&&l->held);l->held=false;}
static bool OSCompareAndSwap(UInt32 old,UInt32 n,volatile UInt32 *p){return __sync_bool_compare_and_swap(p,old,n);}
static UInt32 OSBitOrAtomic(UInt32 mask,volatile UInt32 *p){return __sync_fetch_and_or(p,mask);}
static bool OSCompareAndSwapPtr(void *old,void *n,IOLock **p){return __sync_bool_compare_and_swap(p,(IOLock*)old,(IOLock*)n);}
struct IOWorkLoop:OSObject {
#ifndef A04_ARM64
 pthread_mutex_t mutex;
 IOWorkLoop(){pthread_mutexattr_t a;pthread_mutexattr_init(&a);pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);pthread_mutex_init(&mutex,&a);pthread_mutexattr_destroy(&a);}
 ~IOWorkLoop(){pthread_mutex_destroy(&mutex);}
#endif
 int addEventSource(OSObject*){return 0;}void removeEventSource(OSObject*){}
};
static IOWorkLoop defaultLoop,otherLoop;
struct IOCommandGate:OSObject {using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);OSObject *owner;IOWorkLoop *loop;
 IOReturn runAction(Action f,void *a=NULL,void *b=NULL,void *c=NULL,void *d=NULL){
#ifndef A04_ARM64
 pthread_mutex_lock(&loop->mutex);
#endif
 IOReturn r=f(owner,a,b,c,d);
#ifndef A04_ARM64
 pthread_mutex_unlock(&loop->mutex);
#endif
 return r;}
 IOReturn attemptAction(Action f,void *a=NULL){if(gateDeny)return kIOReturnCannotLock;
#ifndef A04_ARM64
 if(pthread_mutex_trylock(&loop->mutex))return kIOReturnCannotLock;
#endif
 IOReturn r=f(owner,a,NULL,NULL,NULL);
#ifndef A04_ARM64
 pthread_mutex_unlock(&loop->mutex);
#endif
 return r;}
};
struct IOTimerEventSource:OSObject {using Action=void(*)(OSObject*,IOTimerEventSource*);static IOTimerEventSource *timerEventSource(OSObject*,Action){return new IOTimerEventSource;}void setTimeoutMS(unsigned){}void cancelTimeout(){}};
struct IOUserClient;
struct IOService:OSObject {IOService *provider=NULL;bool inactive=false;
 virtual bool start(IOService*){return !startFail;}virtual void stop(IOService*){};
 virtual IOReturn newUserClient(task_t,void*,UInt32,OSDictionary*,IOUserClient**){++nativeFallback;return 77;}
 bool attach(IOService *p){if(attachFail)return false;provider=p;p->retain();return true;}void detach(IOService *p){assert(p==provider);provider=NULL;p->release();}
 ~IOService(){if(provider)provider->release();}IOService *getProvider(){return provider;}bool isInactive(){return inactive;}void terminate(){inactive=true;}
 void setProperty(const char*,bool){}void setProperty(const char*,unsigned long long,unsigned){}void registerService(){}const char *getName(){return "board";}uint64_t getRegistryEntryID(){return 123;}
};
struct IOExternalMethodArguments {uint32_t version=0,selector=0,asyncWakePort=0;void *asyncReference=NULL;uint32_t asyncReferenceCount=0;const uint64_t *scalarInput=NULL;uint32_t scalarInputCount=0;const void *structureInput=NULL;uint32_t structureInputSize=0;void *structureInputDescriptor=NULL;uint64_t *scalarOutput=NULL;uint32_t scalarOutputCount=0;void *structureOutput=NULL;uint32_t structureOutputSize=0;void *structureOutputDescriptor=NULL;uint32_t structureOutputDescriptorSize=0;OSObject **structureVariableOutputData=NULL;};
struct IOExternalMethodDispatch{};
struct IOUserClient:IOService {virtual bool initWithTask(task_t,void*,UInt32){return !initFail;}static IOReturn clientHasPrivilege(void*,int p){assert(p==1);return admin?0:kIOReturnNotPermitted;}virtual IOReturn clientClose(){return 0;}virtual IOReturn clientDied(){return 0;}virtual IOReturn externalMethod(uint32_t,IOExternalMethodArguments*,IOExternalMethodDispatch*=NULL,OSObject*=NULL,void*=NULL){return 77;}};
struct IOHIDEventServiceUserClient:IOUserClient{};
template<class T>static T *allocate(){return clientAllocFail?NULL:new T();}
#define OSTypeAlloc(t) allocate<t>()
static void *current_task(){return (void*)1;}
static bool PE_parse_boot_argn(const char *s,void *out,unsigned){if(s[0]=='a'&&s[6]=='.'){*(int*)out=console;return console!=-1;}return false;}
extern "C" uint64_t mach_absolute_time(){static uint64_t t;return ++t;}
extern "C" void a04br3_keys_init(){++keysInit;}extern "C" void a04br3_touch_init(){++touchInit;}
struct IOHIDEvent:OSObject {int type=0,usage=0,down=0;uint64_t timestamp=0,sender=0;int fields[16]={};OSArray *children=NULL;
 ~IOHIDEvent(){if(children)children->release();}
 static IOHIDEvent *keyboardEvent(uint64_t t,unsigned page,unsigned u,bool d){assert(page==12);if(fail())return NULL;auto *e=new IOHIDEvent;e->type=1;e->usage=u;e->down=d;e->timestamp=t;return e;}
 static IOHIDEvent *digitizerEvent(uint64_t t,unsigned idx,int type,bool range,unsigned pressure,IOFixed x,IOFixed y){if(fail())return NULL;assert(idx==0||idx==2);auto *e=new IOHIDEvent;e->type=type;e->timestamp=t;e->fields[kIOHIDEventFieldDigitizerX]=x;e->fields[kIOHIDEventFieldDigitizerY]=y;(void)range;(void)pressure;return e;}
 void appendChild(IOHIDEvent *e){if(fail())return;if(!children)children=new OSArray;children->setObject(e);}
 OSArray *getChildren(){return children;}void setIntegerValue(unsigned f,int v){assert(f<16);fields[f]=v;}void setFixedValue(unsigned f,IOFixed v){setIntegerValue(f,v);}void setTimeStamp(uint64_t t){timestamp=t;}void setSenderID(uint64_t s){sender=s;}
};
struct IOHIDEventService:IOService {using Action=void(*)(OSObject*,OSObject*,void*,OSObject*,IOOptionBits);IOLock *_clientLock=NULL;OSArray *_clients=NULL;IOWorkLoop *_hidWorkLoop=&defaultLoop;IOCommandGate *_hidGate=NULL;
 bool start(IOService *p) override{if(!IOService::start(p))return false;_clientLock=IOLockAlloc();_clients=new OSArray;_hidGate=new IOCommandGate;_hidGate->owner=this;_hidGate->loop=_hidWorkLoop;return true;}
 ~IOHIDEventService(){if(_clients)_clients->release();if(_hidGate)_hidGate->release();}
 virtual UInt32 getPrimaryUsagePage(){return 0;}virtual UInt32 getPrimaryUsage(){return 0;}virtual UInt32 getProductID(){return 0;}
 void dispatchEvent(IOHIDEvent*,IOOptionBits=0);IOReturn newUserClient(task_t,void*,UInt32,OSDictionary*,IOUserClient**) override;
};
'''.replace('IOExternalMethodDispatch*=NULL,OSObject*=NULL,void*=NULL','IOExternalMethodDispatch * =NULL,OSObject * =NULL,void * =NULL')
POST=r'''
#ifndef A04_ARM64
static unsigned sent,downs,ups;static int callbackMode;static IOUserClient *client;static IOExternalMethodArguments args;static A04BR3HIDButtons *buttons;static A04BR3HIDTouch *touch;
static IOHIDEvent *lastDown,*lastUp;static std::atomic<int> entered{0},physicalFinished{0};
static void callback(OSObject*,OSObject *sender,void*,OSObject *object,IOOptionBits){
 auto *e=dynamic_cast<IOHIDEvent*>(object);assert(e&&e->sender==123);++sent;
 bool down=e->type==1?e->down:e->fields[kIOHIDEventFieldDigitizerTouch];
 if(down){++downs;OSSafeReleaseNULL(lastDown);lastDown=e;e->retain();}else{++ups;OSSafeReleaseNULL(lastUp);lastUp=e;e->retain();}
 if(callbackMode==1&&down){assert(client->externalMethod(0,&args)==kIOReturnBusy);IOUserClient *second=NULL;assert(!buttons->newUserClient((void*)1,(void*)1,A04_UI_CLIENT_TYPE,NULL,&second));assert(second->externalMethod(0,&args)==kIOReturnBusy);second->clientClose();second->release();}
 if(callbackMode==2&&down){assert(!client->clientDied());assert(client->isInactive());}
 if(callbackMode==4&&down){assert(!client->clientDied());client->release();client=NULL;}
 if(callbackMode==3&&down){entered=1;std::this_thread::sleep_for(std::chrono::milliseconds(10));assert(!physicalFinished);}
 assert(sender==buttons||sender==touch);
}
static void connect(IOHIDEventService *s){auto *c=new IOHIDEventServiceClient();assert(c->initClient());c->client=s;c->context=NULL;c->action=callback;s->_clients->setObject(c);c->release();}
static void cleanEvents(){OSSafeReleaseNULL(lastDown);OSSafeReleaseNULL(lastUp);sent=downs=ups=0;}
static IOUserClient *open(){IOUserClient *c=NULL;assert(!buttons->newUserClient((void*)1,(void*)1,A04_UI_CLIENT_TYPE,NULL,&c)&&c);return c;}
static void failOpen(int expected){int n=alive,b=buttons->refs;IOUserClient *c=(IOUserClient*)1;assert(buttons->newUserClient((void*)1,(void*)1,A04_UI_CLIENT_TYPE,NULL,&c)==expected&&!c);assert(n==alive&&b==buttons->refs);}
int main(){
 const int baseline=alive;buttons=new A04BR3HIDButtons();touch=new A04BR3HIDTouch();assert(buttons->start(NULL)&&touch->start(NULL));assert(keysInit==1&&touchInit==1);connect(buttons);connect(touch);
 uint64_t request[3]={A04_UI_HOME,0,0};args.scalarInput=request;args.scalarInputCount=3;
 console=-1;failOpen(kIOReturnNotPermitted);console=0;failOpen(kIOReturnNotPermitted);console=2;failOpen(kIOReturnNotPermitted);console=1;
 admin=0;failOpen(kIOReturnNotPermitted);admin=1;clientAllocFail=1;failOpen(kIOReturnNoMemory);clientAllocFail=0;initFail=1;failOpen(kIOReturnBadArgument);initFail=0;attachFail=1;failOpen(kIOReturnUnsupported);attachFail=0;startFail=1;failOpen(kIOReturnUnsupported);startFail=0;
 IOUserClient *other=NULL;assert(touch->newUserClient((void*)1,(void*)1,A04_UI_CLIENT_TYPE,NULL,&other)==kIOReturnUnsupported&&!other);
 assert(buttons->newUserClient(NULL,NULL,A04_UI_CLIENT_TYPE,NULL,NULL)==kIOReturnBadArgument);
 assert(buttons->newUserClient(NULL,NULL,0,NULL,&other)==77&&nativeFallback==1);
 assert(!buttons->newUserClient(NULL,NULL,kIOHIDEventServiceUserClientType,NULL,&other));assert(dynamic_cast<IOHIDEventServiceUserClient*>(other));other->release();
 client=open();assert(!client->externalMethod(0,&args));assert(sent==2&&downs==1&&ups==1&&lastDown->usage==64&&lastUp->usage==64&&lastUp->timestamp>lastDown->timestamp);cleanEvents();
 admin=0;assert(client->externalMethod(0,&args)==kIOReturnNotPermitted);admin=1;console=0;assert(client->externalMethod(0,&args)==kIOReturnNotPermitted);console=1;
 auto saved=args;args.scalarInput=NULL;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);args=saved;args.scalarInputCount=2;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);args=saved;
 assert(client->externalMethod(1,&args)==kIOReturnBadArgument);assert(client->externalMethod(0,NULL)==kIOReturnBadArgument);
 for(unsigned n=0;n<8;n++){args=saved;switch(n){case 0:args.scalarOutputCount=1;break;case 1:args.structureInputSize=1;break;case 2:args.structureOutputSize=1;break;case 3:args.structureInputDescriptor=(void*)1;break;case 4:args.structureOutputDescriptor=(void*)1;break;case 5:args.asyncWakePort=1;break;case 6:args.asyncReferenceCount=1;break;case 7:args.structureVariableOutputData=(OSObject**)1;break;}assert(client->externalMethod(0,&args)==kIOReturnBadArgument);}args=saved;
 for(uint64_t bad: {0ULL,3ULL,0xffffffffffffffffULL}){request[0]=bad;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);}request[0]=A04_UI_HOME;request[1]=1;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);request[1]=0;
 for(int fail=1;fail<=2;fail++){failAt=fail;allocAt=0;assert(client->externalMethod(0,&args)==kIOReturnNoMemory&&!sent);failAt=0;}
 request[0]=A04_UI_TAP;
 for(int x: {0,799})for(int y: {0,1279}){request[1]=x;request[2]=y;assert(!client->externalMethod(0,&args));assert(sent==2&&downs==1&&ups==1);auto *child=(IOHIDEvent*)lastDown->children->getObject(0);assert(child->fields[kIOHIDEventFieldDigitizerIdentity]==2&&child->fields[kIOHIDEventFieldDigitizerTouch]==1);assert(lastDown->fields[kIOHIDEventFieldDigitizerX]==(x<<16)/800&&lastUp->fields[kIOHIDEventFieldDigitizerY]==(y<<16)/1280);assert(lastDown->fields[kIOHIDEventFieldDigitizerIsDisplayIntegrated]&&lastDown->fields[kIOHIDEventFieldIsBuiltIn]&&lastDown->fields[kIOHIDEventFieldDigitizerCollection]);assert(!lastUp->fields[kIOHIDEventFieldDigitizerRange]&&!lastUp->fields[kIOHIDEventFieldDigitizerButtonMask]);cleanEvents();}
 for(auto xy: {0,1}){request[1]=xy?799:800;request[2]=xy?1280:1279;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);}request[1]=UINT64_MAX;request[2]=0;assert(client->externalMethod(0,&args)==kIOReturnBadArgument);request[1]=400;request[2]=640;
 for(int fail=1;fail<=6;fail++){failAt=fail;allocAt=0;int n=alive;assert(client->externalMethod(0,&args)==kIOReturnNoMemory&&!sent&&alive==n);failAt=0;}
 gateDeny=1;assert(client->externalMethod(0,&args)==kIOReturnCannotLock&&!sent);gateDeny=0;
 touch->_hidWorkLoop=&otherLoop;assert(client->externalMethod(0,&args)==kIOReturnNotReady&&!sent);touch->_hidWorkLoop=&defaultLoop;
 // All physical inputs use the exact production C -> gate -> event pipeline.
 for(int k=0;k<4;k++){a04br3_hid_key(k,1);cleanEvents();assert(client->externalMethod(0,&args)==kIOReturnBusy&&!sent);a04br3_hid_key(k,0);cleanEvents();}
 a04br3_hid_touch(0,1,500,600);cleanEvents();assert(client->externalMethod(0,&args)==kIOReturnBusy&&!sent);a04br3_hid_touch(0,0,500,600);cleanEvents();
 failAt=1;allocAt=0;a04br3_hid_touch(0,1,500,600);failAt=0;assert(client->externalMethod(0,&args)==kIOReturnBusy&&!sent);a04br3_hid_touch(0,0,500,600);cleanEvents();
 // Native clamp/move/duplicate/release positions retain the established shape.
 a04br3_hid_touch(0,1,-5,3000);assert(sent==1&&lastDown->fields[kIOHIDEventFieldDigitizerX]==0&&lastDown->fields[kIOHIDEventFieldDigitizerY]==(1279<<16)/1280);cleanEvents();
 a04br3_hid_touch(0,1,800,-9);assert(sent==1);auto *movedChild=(IOHIDEvent*)lastDown->children->getObject(0);assert(movedChild->fields[kIOHIDEventFieldDigitizerEventMask]==(kIOHIDDigitizerEventFromEdgeTip|kIOHIDDigitizerEventPosition));cleanEvents();
 a04br3_hid_touch(0,1,799,0);assert(!sent);
 a04br3_hid_touch(0,0,300,400);assert(sent==1&&lastUp->fields[kIOHIDEventFieldDigitizerX]==(300<<16)/800);auto *upChild=(IOHIDEvent*)lastUp->children->getObject(0);assert(upChild->fields[kIOHIDEventFieldDigitizerX]==(799<<16)/800&&upChild->fields[kIOHIDEventFieldDigitizerY]==0);cleanEvents();
 a04br3_hid_touch(1,1,0,0);a04br3_hid_key(4,1);assert(!sent);
 callbackMode=1;assert(!client->externalMethod(0,&args)&&sent==2);callbackMode=0;cleanEvents();
 callbackMode=2;assert(!client->externalMethod(0,&args)&&sent==2);assert(client->externalMethod(0,&args)==kIOReturnBusy);callbackMode=0;cleanEvents();client->release();client=open();
 // Stop/detach cannot destroy the provider while the private client remains.
 client->stop(buttons);client->detach(buttons);assert(client->externalMethod(0,&args)==kIOReturnBusy);client->release();client=open();
 // The last connection reference can disappear during down; the method's
 // retain keeps both client and explicit provider alive through release.
 callbackMode=4;auto *inflight=client;assert(!inflight->externalMethod(0,&args)&&!client&&sent==2);callbackMode=0;cleanEvents();client=open();
 // A hardware arrival waits for the gate, then is delivered after release.
 callbackMode=3;std::thread physical([]{while(!entered)std::this_thread::yield();a04br3_hid_key(3,1);physicalFinished=1;});assert(!client->externalMethod(0,&args));physical.join();callbackMode=0;assert(sent==3&&downs==2&&ups==1&&physicalFinished);a04br3_hid_key(3,0);cleanEvents();
 touch->stop(NULL);assert(client->externalMethod(0,&args)==kIOReturnNotReady&&!sent);touch->release();buttons->stop(NULL);assert(client->externalMethod(0,&args)==kIOReturnNotReady&&!sent);client->clientClose();client->release();buttons->release();assert(alive==baseline);
 printf("PASS private dispatch/native input/allocation rollback/close death/gate concurrency/refcounts\n");
}
#endif
'''

def actual_source():
    source=(HID/'A04BR3HID.cpp').read_text()
    client=source[source.index('class IOHIDEventServiceClient :'):source.index('#define super IOService')]
    dispatch=source[source.index('void\nIOHIDEventService::dispatchEvent('):source.index('\nIOHIDEvent *\nIOHIDEventService::copyEvent(')]
    uc=source[source.index('IOReturn\nIOHIDEventService::newUserClient('):source.index('/*\n * Buttons.')]
    return STUB+'\n#include "A04BR3UITest.h"\n'+client+'\n'+dispatch+'\nstatic IOReturn a04_ui_open(IOHIDEventService *,task_t,void *,UInt32,OSDictionary *,IOUserClient **);\n#define super IOService\n'+uc+'\n'+source[source.index('class A04BR3HIDButtons :'):]+POST

class UITest(unittest.TestCase):
    def test_actual_dispatch_failures_lifecycle_and_arm64(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);f=p/'test.cpp';f.write_text(actual_source());exe=p/'test'
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-pthread','-I',str(HID),str(f),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
            subprocess.run(['clang++','--target=arm64-apple-ios16.0','-std=c++17','-ffreestanding','-DA04_ARM64','-Wall','-Wextra','-Werror','-Wno-unused-variable','-I',str(HID),'-c',str(f),'-o',str(p/'test.o')],check=True)

if __name__=='__main__':unittest.main()
