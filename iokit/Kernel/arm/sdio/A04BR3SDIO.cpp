/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 MSDC2/MT6630 bring-up, based on board register definitions.
 * Deliberately no IO80211 service or connected state until data path exists.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
#include "A04BR3SDIOPIO.h"
#include "A04BR3SDIOPads.h"
#include "A04BR3WMTRoundtrip.h"
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
class A04BR3SDIO : public IOService {
 OSDeclareDefaultStructors(A04BR3SDIO);
 vm_offset_t host,top,gpio;
 IOLock *transportLock=nullptr;
 struct a04_sdio_io io;
 struct a04_sdio_transport transport;
 bool transportReady=false;
 struct a04_wmt_channel channel;
 static uint32_t dataRead(void *p,unsigned off,unsigned width){
  A04BR3SDIO *s=(A04BR3SDIO *)p;
  if(width==4)return rd(s->host,off);
  uint32_t v=*(volatile uint8_t *)(s->host+off);__asm__ volatile("dmb oshld":::"memory");return v;
 }
 static void dataWrite(void *p,unsigned off,uint32_t v,unsigned width){
  A04BR3SDIO *s=(A04BR3SDIO *)p;
  if(width==4){wr(s->host,off,v);return;}
  *(volatile uint8_t *)(s->host+off)=(uint8_t)v;__asm__ volatile("dsb oshst":::"memory");
 }
 static void dataDelay(void *,unsigned us){IODelay(us);}
 static int transfer(void *p,uint32_t arg,void *buffer,unsigned length){
  struct a04_sdio_regs regs={p,dataRead,dataWrite,dataDelay};
  int e=a04_sdio_pio(&regs,arg,buffer,length);
  if(e)IOLog("A04BR3 SDIO: CMD53 error=%d arg=0x%x len=%u status=0x%x fifo=0x%x (transport stopped)\n",e,arg,length,rd(((A04BR3SDIO *)p)->host,0x0c),rd(((A04BR3SDIO *)p)->host,0x14));
  return e;
 }
 static uint32_t rd(vm_offset_t b,unsigned o){uint32_t v=*(volatile uint32_t *)(b+o);__asm__ volatile("dmb oshld":::"memory");return v;}
 static void wr(vm_offset_t b,unsigned o,uint32_t v){*(volatile uint32_t *)(b+o)=v;__asm__ volatile("dsb oshst":::"memory");}
 static bool wait(vm_offset_t b,unsigned o,uint32_t mask,uint32_t value){for(unsigned n=0;n<10000;n++){if((rd(b,o)&mask)==value)return true;IODelay(10);}return false;}
 void pin(unsigned p,unsigned mode){unsigned o=0x300+(p/5)*16,s=(p%5)*3;wr(gpio,o,(rd(gpio,o)&~(7u<<s))|(mode<<s));}
 void output(unsigned p,bool high){pin(p,0);wr(gpio,(high?0x104:0x108)+(p/32)*16,1u<<(p%32));wr(gpio,4+(p/32)*16,1u<<(p%32));}
 static void padWrite(void *p,unsigned off,uint32_t v){wr(((A04BR3SDIO *)p)->gpio,off,v);}
 static int command(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *out){
  A04BR3SDIO *s=(A04BR3SDIO *)p;
  if(!wait(s->host,0x3c,1,0))return 1;
  wr(s->host,0x0c,rd(s->host,0x0c));wr(s->host,0x38,arg);wr(s->host,0x34,op|(response<<7));
  for(unsigned n=0;n<10000;n++){
   uint32_t st=rd(s->host,0x0c)&0x700;
   if(st){wr(s->host,0x0c,st);if(st&0x600){IOLog("A04BR3 SDIO: CMD%u arg=0x%x interrupt=0x%x pins=0x%x cfg=0x%x\n",op,arg,st,rd(s->host,8),rd(s->host,0));return 2;}*out=rd(s->host,0x40);if(response==7&&!wait(s->host,0x3c,1,0))return 3;return 0;}
   IODelay(10);
  }
  return 4;
 }
 static void delay(void *,unsigned us){IOSleep((us+999)/1000);}
public:
 bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
 void stop(IOService *provider) APPLE_KEXT_OVERRIDE {
  if(transportLock){IOLockLock(transportLock);transportReady=false;IOLockUnlock(transportLock);}
  setProperty("WMTTransportReady",kOSBooleanFalse);
  IOService::stop(provider);
 }
 void free(void) APPLE_KEXT_OVERRIDE {
  if(transportLock)IOLockFree(transportLock);
  IOService::free();
 }
 /* Synchronous kernel transport entry points. Never call from interrupt context.
  * Function1 is enabled on demand; WMT preparation only touches function2. */
 int enableFunction(unsigned fn){
  if(!transportLock)return A04_SDIO_POISONED;
  IOLockLock(transportLock);
  int e=transportReady?a04_sdio_enable(&transport,fn):A04_SDIO_POISONED;
  IOLockUnlock(transportLock);return e;
 }
 int transferFunction(unsigned fn,unsigned address,int write,int increment,void *buffer,unsigned length){
  if(!transportLock)return A04_SDIO_POISONED;
  IOLockLock(transportLock);
  /* fn2 belongs to the serialized WMT channel: raw CHISR/FIFO accesses
   * would consume status/data behind its credit and response bookkeeping. */
  int e=!transportReady?A04_SDIO_POISONED:
   (fn==2 && channel.initialized)?A04_SDIO_ARGUMENT:
   a04_sdio_transfer(&transport,fn,address,write,increment,buffer,length);
  if(transport.failed){transportReady=false;setProperty("WMTTransportReady",kOSBooleanFalse);}
  IOLockUnlock(transportLock);return e;
 }
};
OSDefineMetaClassAndStructors(A04BR3SDIO,IOService);
bool A04BR3SDIO::start(IOService *provider)
{
 unsigned enabled=0;
 if(!PE_parse_boot_argn("a04br3.display_probe",&enabled,sizeof(enabled))||enabled!=1||!IOService::start(provider))return false;
 static bool attempted=false;
 if(__atomic_exchange_n(&attempted,true,__ATOMIC_RELAXED))return false;
 top=ml_io_map(0x10000000,0x1000);gpio=ml_io_map(0x10005000,0x1000);if(!top||!gpio)return false;
 /* Select 26MHz with source gated. TOP2 bit30 is an inverted gate. */
 wr(top,0xa0,2);wr(top,0x40,rd(top,0x40)&~(7u<<3));wr(top,0x9c,1);wr(top,0x6c,1u<<30);wr(top,0xb0,2);
 host=ml_io_map(0x11170000,0x1000);if(!host)return false;
 IOLog("A04BR3 SDIO: MSDC2 inherited cfg=0x%x GPIO=0x%x/0x%x\n",rd(host,0),rd(gpio,0x100),rd(gpio,0x200));
 /* External combo PMU GPIO4 and reset GPIO7; retain board RTC source.
  * Do not touch UART or the separate eMMC controller. */
 output(4,false);output(7,false);IOSleep(10);output(4,true);IOSleep(30);output(7,true);IOSleep(30);
 for(unsigned p=68;p<=73;p++)pin(p,1);
 a04_sdio_pad_pulls(this,padWrite);
 wr(gpio,0x934,0x7e); /* MSDC2 input enables; board IES offsets are +4 set. */
 wr(host,0x10,0);wr(host,0,rd(host,0)|4);
 if(!wait(host,0,4,0)){IOLog("A04BR3 SDIO: reset timeout\n");return false;}
 wr(host,0,(rd(host,0)&~((3u<<20)|(0xfffu<<8)|2u))|9u|(17u<<8));
 if(!wait(host,0,0x80,0x80)){IOLog("A04BR3 SDIO: clock timeout cfg=0x%x\n",rd(host,0));return false;}
 wr(host,0x30,(rd(host,0x30)&~(3u<<16))|(1u<<19)|(0xffu<<24));
 IOSleep(10);
 io={this,command,delay};struct a04_sdio_card card={};int error=a04_sdio_identify(&io,&card);
 IOLog("A04BR3 SDIO: identify=%d OCR=0x%x RCA=0x%x functions=%u CCCR=0x%x vendor=0x%x device=0x%x\n",error,card.ocr,card.rca,card.functions,card.revision,card.vendor,card.device);
 if(error)return false;
 if(card.vendor!=0x037a || card.device!=0x6630 || card.functions<2)return false;
 transportLock=IOLockAlloc();if(!transportLock)return false;
 transport={&io,transfer,card.functions,0,0};
 struct a04_sdio_wmt_state wmt={};
 error=a04_sdio_wmt_prepare(&transport,&wmt);
 IOLog("A04BR3 SDIO: WMT fn2 prepare=%d CCIR=0x%x CHLPCR=0x%x->0x%x polls=%u enabled=0x%x\n",error,wmt.ccir,wmt.chlpcr_before,wmt.chlpcr_after,wmt.polls,transport.enabled);
 if(error)return false;
 /* Keep all WMT commands and destructive CHISR/CRDR reads under one lock.
  * Cold preparation above is the only point at which FIFO credits initialize. */
 IOLockLock(transportLock);
 error=a04_wmt_channel_init(&transport,&channel);
 IOLog("A04BR3 WMT: channel init=%d CHCR=0x%x (polled)\n",error,channel.chcr);
 const uint32_t addresses[2]={0x80000000u,0x80000004u};
 uint32_t versions[2]={0,0};
 for(unsigned i=0;!error && i<2;i++){
  error=a04_wmt_read_version(&transport,&channel,addresses[i],&versions[i]);
  IOLog("A04BR3 WMT: version addr=0x%x error=%d value=0x%x CHISR=0x%x polls=%u rxlen=%u packets=%u event=%u credit=%u pending=%u\n",addresses[i],error,versions[i],channel.status,channel.polls,channel.rx_length,channel.rx_packets,channel.event_length,channel.free_bytes,channel.pending_cost);
  if(channel.event_length){
   IOLog("A04BR3 WMT: event");
   for(unsigned k=0;k<channel.event_length && k<32;k++)IOLog(" %02x",channel.event[k]);
   IOLog("\n");
  }
 }
 transportReady=!error;
 IOLockUnlock(transportLock);
 setProperty("WMTVersionQueryError",(unsigned)error,32);
 if(!error){
  setProperty("WMTHardwareVersion",versions[0],32);setProperty("WMTFirmwareROMVersion",versions[1],32);
  IOLog("A04BR3 WMT: identity HW=0x%x FW/ROM=0x%x; firmware selection pending validated blob/revision match\n",versions[0],versions[1]);
 }
 /* Preserve the diagnostic service on query failure but expose no readiness.
  * Firmware body transfer and WLAN startup remain gated until independently
  * selected firmware and version-query hardware validation are available. */
 setProperty("WMTTransportReady",transportReady?kOSBooleanTrue:kOSBooleanFalse);
 setProperty("WMTCCIR",wmt.ccir,32);setProperty("WMTCHLPCR",wmt.chlpcr_after,32);
 setProperty("SDIOVendor",card.vendor,32);setProperty("SDIODevice",card.device,32);setProperty("NetworkDataPathAvailable",kOSBooleanFalse);registerService();return true;
}
