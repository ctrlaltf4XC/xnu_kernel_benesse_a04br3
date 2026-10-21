/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 MSDC2/MT6630 bring-up, based on board register definitions.
 * Deliberately no IO80211 service or connected state until data path exists.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
#include "A04BR3SDIOPIO.h"
#include "A04BR3SDIOPads.h"
#include "A04BR3WMTDownload.h"
#include "A04BR3WMTBootFirmware.h"
#include "A04BR3WLANFirmware.h"
#include "A04BR3WLANBootFirmware.h"
#include <libkern/crypto/sha2.h>
#include <libkern/c++/OSData.h>
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
class A04BR3SDIO : public IOService {
 OSDeclareDefaultStructors(A04BR3SDIO);
 vm_offset_t host,top,gpio;
 IOLock *transportLock=nullptr;
 struct a04_sdio_io io;
 struct a04_sdio_transport transport;
 bool transportReady=false;
 struct a04_wmt_channel channel;
 struct a04_wmt_identity identity={};
 bool identityReady=false;
 void *firmwareSnapshot=nullptr;
 unsigned firmwareBytes=0;
 void *wlanSnapshot=nullptr;
 unsigned wlanBytes=0;
 bool wlanAttempted=false;
 struct a04_wlan_host *wlanHost=nullptr;
 struct a04_wlan_firmware wlanFirmware={};
 struct a04_wlan_download_state wlanDownload={};
 struct a04_wmt_firmware_set firmwareSet={};
 struct a04_wmt_download_state downloadState={};
 struct a04_wmt_startup_options startupOptions={0,0,0,1,2,1};
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
  if(transportLock){IOLockLock(transportLock);identityReady=false;transportReady=false;IOLockUnlock(transportLock);}
  setProperty("WMTTransportReady",kOSBooleanFalse);
  setProperty("WLANFirmwareReady",kOSBooleanFalse);
  IOService::stop(provider);
 }
 void free(void) APPLE_KEXT_OVERRIDE {
  if(firmwareSnapshot)IOFree(firmwareSnapshot,firmwareBytes);
  if(wlanSnapshot)IOFree(wlanSnapshot,wlanBytes);
  if(wlanHost)IOFree(wlanHost,sizeof(*wlanHost));
  if(transportLock)IOLockFree(transportLock);
  IOService::free();
 }
 /* Explicit CPU-view staging interface. Parent owns delivery/reservation;
  * caller retains input until return. No PA translation, GPU asset aliasing,
  * firmware transmission, resets or automatic retries occur here. */
 int stageWMTFirmware(const void *bytes,unsigned length){
  if(!bytes || length<32 || length>A04_WMT_FW_MAX_BUNDLE)return A04_WMT_FW_ARGUMENT;
  if(!transportLock)return A04_WMT_FW_STATE;
  IOLockLock(transportLock);
  int e=0;
  if(!identityReady || !transportReady || transport.failed || firmwareSnapshot)e=A04_WMT_FW_STATE;
  void *snapshot=nullptr;
  if(!e){snapshot=IOMalloc(length);if(!snapshot)e=A04_WMT_FW_MEMORY;}
  if(!e){
   bcopy(bytes,snapshot,length);
   e=a04_wmt_firmware_bundle(&identity,snapshot,length,&firmwareSet);
  }
  if(!e){firmwareSnapshot=snapshot;firmwareBytes=length;}
  else if(snapshot){firmwareSet.count=0;IOFree(snapshot,length);}
  setProperty("WMTFirmwareStageError",(unsigned)e,32);
  setProperty("WMTFirmwareStaged",firmwareSnapshot?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("WMTFirmwareRunning",kOSBooleanFalse);
  IOLog("A04BR3 WMT: firmware CPU stage=%d bundle=%u parts=%u HW=0x%x ROM=0x%x; no firmware sent\n",e,length,firmwareSet.count,identity.hardware,identity.rom);
  if(!e)for(unsigned i=0;i<firmwareSet.count;i++){
   const struct a04_wmt_firmware_file *f=&firmwareSet.files[i];
   IOLog("A04BR3 WMT: staged part=%u/%u body=%u destination=0x%x headerHW=0x%x SW=0x%x\n",f->sequence,f->count,f->header.body.length,f->destination,f->header.hardware,f->header.software);
  }
  IOLockUnlock(transportLock);return e;
 }
 static void downloadProgress(void *p,const struct a04_wmt_download_state *d){
  A04BR3SDIO *s=(A04BR3SDIO *)p;
  if(d->phase==A04_WMT_DL_FRAGMENT && (d->fragment&15))return;
  IOLog("A04BR3 WMT: download phase=%u part=%u/%u fragment=%u offset=%u commands=%u ACKs=%u resets=%u credit=%u\n",d->phase,d->sequence,s->firmwareSet.count,d->fragment,d->offset,d->commands,d->acked,d->reset_acks,s->channel.free_bytes);
 }
 int startWMTFirmware(){
  if(!transportLock)return A04_WMT_DOWNLOAD_STATE;
  IOLockLock(transportLock);
  if(downloadState.attempted){IOLockUnlock(transportLock);return A04_WMT_DOWNLOAD_STATE;}
  int e=(!identityReady || !transportReady || !firmwareSnapshot)?A04_WMT_DOWNLOAD_STATE:
   a04_wmt_download_run(&transport,&channel,&firmwareSet,&identity,&startupOptions,&downloadState,downloadProgress,this);
  if(transport.failed)transportReady=false;
  setProperty("WMTFirmwareDownloadError",(unsigned)e,32);
  setProperty("WMTFirmwareDownloadComplete",(!e && downloadState.phase==A04_WMT_DL_READY)?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("WMTSTPReady",(!e && downloadState.stp_ready)?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("WMTWiFiFunctionReady",(!e && downloadState.function1_ready)?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("WMTTransportReady",transportReady?kOSBooleanTrue:kOSBooleanFalse);
  /* fn1 enabled is a WLAN probe boundary, not evidence of WLAN FW execution. */
  setProperty("WMTFirmwareRunning",kOSBooleanFalse);
  IOLog("A04BR3 WMT: download result=%d phase=%u part=%u fragment=%u offset=%u ACKs=%u/%u resets=%u parts=%u STP=%u fn1=%u CHISR=0x%x credit=%u pending=%u\n",e,downloadState.phase,downloadState.sequence,downloadState.fragment,downloadState.offset,downloadState.acked,downloadState.commands,downloadState.reset_acks,downloadState.parts_acked,downloadState.stp_ready,downloadState.function1_ready,channel.status,channel.free_bytes,channel.pending_cost);
  if(e && channel.event_length){
   IOLog("A04BR3 WMT: failing event");
   for(unsigned i=0;i<channel.event_length && i<32;i++)IOLog(" %02x",channel.event[i]);
   IOLog("\n");
  }
  IOLockUnlock(transportLock);return e;
 }
 /* Explicit immutable CPU copy; no firmware file probing or raw PA aliasing.
  * SHA256 pins the supplied stock asset, CRC validates its divided layout.
  * WMT ID8a12/FVR8a11 + completed patch/STP/fn1 are required independently. */
 int stageWLANFirmware(const void *bytes,unsigned length){
  if(!transportLock || !bytes || length!=237008)return A04_WLAN_ARGUMENT;
  IOLockLock(transportLock);
  int e=(!identityReady || !transportReady || transport.failed || wlanSnapshot || wlanAttempted ||
   downloadState.phase!=A04_WMT_DL_READY || !downloadState.function1_ready)?A04_WLAN_STATE:0;
  void *snapshot=nullptr;uint8_t digest[SHA256_DIGEST_LENGTH]={};
  if(!e){snapshot=IOMalloc(length);if(!snapshot)e=A04_WMT_FW_MEMORY;}
  if(!e){
   bcopy(bytes,snapshot,length);SHA256_CTX hash;
   SHA256_Init(&hash);SHA256_Update(&hash,snapshot,length);SHA256_Final(digest,&hash);
   e=a04_wlan_firmware_select(&identity,snapshot,length,digest,&wlanFirmware);
  }
  if(!e){wlanSnapshot=snapshot;wlanBytes=length;}
  else if(snapshot){wlanFirmware.count=0;IOFree(snapshot,length);}
  setProperty("WLANFirmwareStageError",(unsigned)e,32);
  setProperty("WLANFirmwareStaged",wlanSnapshot?kOSBooleanTrue:kOSBooleanFalse);
  IOLog("A04BR3 WLAN: CPU stage=%d bytes=%u sections=%u candidate=%u HW=0x%x ROM=0x%x\n",e,length,wlanFirmware.count,wlanFirmware.candidate,identity.hardware,identity.rom);
  if(!e)for(unsigned i=0;i<wlanFirmware.count;i++)
   IOLog("A04BR3 WLAN: section=%u address=0x%x bytes=%u\n",i+1,wlanFirmware.section[i].address,wlanFirmware.section[i].length);
  IOLockUnlock(transportLock);return e;
 }
 int startWLANFirmware(){
  if(!transportLock)return A04_WLAN_STATE;
  IOLockLock(transportLock);
  if(wlanAttempted || !wlanSnapshot || !transportReady || transport.failed ||
     downloadState.phase!=A04_WMT_DL_READY || !downloadState.function1_ready){IOLockUnlock(transportLock);return A04_WLAN_STATE;}
  wlanHost=(struct a04_wlan_host *)IOMalloc(sizeof(*wlanHost));
  if(!wlanHost){
   setProperty("WLANFirmwareDownloadError",A04_WMT_FW_MEMORY,32);
   IOLog("A04BR3 WLAN: host buffer allocation failed; no fn1 transaction attempted\n");
   IOLockUnlock(transportLock);return A04_WMT_FW_MEMORY;
  }
  bzero(wlanHost,sizeof(*wlanHost));
  wlanAttempted=true;
  int e=a04_wlan_host_open(wlanHost,&transport);
  IOLog("A04BR3 WLAN: fn1 prepare=%d WCIR=0x%x WHLPCR=0x%x own-polls=%u enabled=0x%x\n",e,wlanHost->wcir,wlanHost->chlpcr,wlanHost->own_polls,transport.enabled);
  if(!e)e=a04_wlan_download(wlanHost,&wlanFirmware,&wlanDownload);
  if(transport.failed)transportReady=false;
  bool ready=!e && wlanDownload.ready && wlanHost->ready;
  setProperty("WLANFirmwareDownloadError",(unsigned)e,32);
  setProperty("WLANFirmwareReady",ready?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("WLANWCIR",wlanHost->wcir,32);
  setProperty("WMTTransportReady",transportReady?kOSBooleanTrue:kOSBooleanFalse);
  setProperty("NetworkDataPathAvailable",kOSBooleanFalse);
  IOLog("A04BR3 WLAN: download result=%d attempted=%u section=%u/%u offset=%u configs=%u packets=%u start=%u ready=%u WCIR=0x%x rxlen=%u CPU-free=%u used=%u transport-error=%d\n",e,wlanDownload.attempted,wlanDownload.section,wlanFirmware.count,wlanDownload.offset,wlanDownload.configured,wlanDownload.packets,wlanDownload.started,ready?1u:0u,wlanHost->wcir,wlanHost->rx_length,wlanHost->cpu_free,wlanHost->cpu_used,transport.failed);
  if(e && wlanHost->rx_length){
   IOLog("A04BR3 WLAN: failing init event");
   for(unsigned i=0;i<wlanHost->rx_length && i<32;i++)IOLog(" %02x",wlanHost->rx[i]);
   IOLog("\n");
  }
  IOLockUnlock(transportLock);return e;
 }
 void loadBootWLANFirmware(IOService *provider){
  setProperty("WLANFirmwareReady",kOSBooleanFalse);
  const void *view=nullptr;unsigned bytes=0;
  int boot=a04_wlan_boot_firmware_view(&view,&bytes),stage=A04_WLAN_STATE;
  setProperty("WLANBootFirmwareError",(unsigned)boot,32);
  IOLog("A04BR3 WLAN: boot view=%d bytes=%u\n",boot,bytes);
  if(!boot)stage=stageWLANFirmware(view,bytes);
  else if(boot==1){
   OSObject *object=provider->copyProperty("WLANFirmware-A04BR3");
   OSData *asset=OSDynamicCast(OSData,object);
   if(asset)stage=stageWLANFirmware(asset->getBytesNoCopy(),asset->getLength());
   else if(object)IOLog("A04BR3 WLAN: rejected non-OSData CPU firmware\n");
   else IOLog("A04BR3 WLAN: explicit asset absent; WLAN download deferred\n");
   if(object)object->release();
  }else IOLog("A04BR3 WLAN: invalid DT range/mapping; no fallback\n");
  if(!stage)startWLANFirmware();
 }
 int copyWMTFirmware(unsigned sequence,unsigned offset,void *bytes,unsigned length){
  if(!transportLock)return A04_WMT_FW_STATE;
  IOLockLock(transportLock);
  int e=identityReady && firmwareSnapshot?
   a04_wmt_firmware_read(&firmwareSet,sequence,offset,bytes,length):A04_WMT_FW_STATE;
  IOLockUnlock(transportLock);return e;
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
   ((fn==2 && channel.initialized) || (fn==1 && wlanHost && wlanHost->initialized))?A04_SDIO_ARGUMENT:
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
 if(!error){identity={0x6630,versions[0]&0xffff,versions[1]&0xffff};identityReady=true;}
 IOLockUnlock(transportLock);
 setProperty("WMTVersionQueryError",(unsigned)error,32);
 if(!error){
  setProperty("WMTHardwareVersion",versions[0],32);setProperty("WMTFirmwareROMVersion",versions[1],32);
  IOLog("A04BR3 WMT: identity HW=0x%x FW/ROM=0x%x; awaiting explicit firmware bundle\n",versions[0],versions[1]);
  struct a04_wmt_revision revision={};
  int re=a04_wmt_revision_decode(&identity,&revision);
  IOLog("A04BR3 WMT: revision decode=%d exact=%u table=0x%x tableECO=%u patch-family=%u (selection uses ROM-low8)\n",re,revision.exact,revision.table_hardware,revision.eco,revision.patch_family);
  setProperty("WMTFirmwareStaged",kOSBooleanFalse);
  setProperty("WMTFirmwareRunning",kOSBooleanFalse);
  /* Explicit options reflect vendor cfgExist/clock/build/FM/HIF choices.
   * Absent config: native cfgExist0 + co-clock disabled, MT6630 non-MT2601
   * merge build enabled, launcher FM_COMM2, fn1 host probe boundary. */
  OSObject *configObject=provider->copyProperty("WMTStartup-A04BR3");
  OSData *config=OSDynamicCast(OSData,configObject);
  bool configValid=!configObject;
  if(config && config->getLength()==24){
   const uint8_t *p=(const uint8_t *)config->getBytesNoCopy();
   startupOptions={a04_wmt_get32(p),a04_wmt_get32(p+4),a04_wmt_get32(p+8),a04_wmt_get32(p+12),a04_wmt_get32(p+16),a04_wmt_get32(p+20)};
   configValid=startupOptions.coex_present<=1 && startupOptions.antenna_mode<=255 && startupOptions.co_clock<=1 && startupOptions.merge_interface<=1 && (startupOptions.fm_strap==1 || startupOptions.fm_strap==2) && startupOptions.function1<=1;
  }
  if(configObject)configObject->release();
  const void *bootView=nullptr;unsigned bootBytes=0;
  int bootError=a04_wmt_boot_firmware_view(&bootView,&bootBytes);
  IOLog("A04BR3 WMT: boot bundle view=%d bytes=%u config-valid=%u\n",bootError,bootBytes,configValid?1u:0u);
  int stageError=A04_WMT_FW_STATE;
  if(!configValid){
   setProperty("WMTFirmwareStageError",A04_WMT_DOWNLOAD_CONFIG,32);
   IOLog("A04BR3 WMT: invalid startup options; firmware send blocked\n");
  }else if(bootError==A04_WMT_BOOT_OK){
   stageError=stageWMTFirmware(bootView,bootBytes);
  }else if(bootError==A04_WMT_BOOT_ABSENT){
   /* Preserve explicit CPU-stage API/property; never reinterpret a DT PA pair
    * as CPU bytes and never fall back after an invalid DT reservation. */
   OSObject *assetObject=provider->copyProperty("WMTFirmware-A04BR3");
   OSData *asset=OSDynamicCast(OSData,assetObject);
   if(asset)stageError=stageWMTFirmware(asset->getBytesNoCopy(),asset->getLength());
   else if(assetObject){
    setProperty("WMTFirmwareStageError",A04_WMT_FW_ARGUMENT,32);
    IOLog("A04BR3 WMT: rejected non-OSData CPU bundle\n");
   }else IOLog("A04BR3 WMT: firmware bundle absent; download deferred\n");
   if(assetObject)assetObject->release();
  }else{
   setProperty("WMTFirmwareStageError",A04_WMT_FW_RANGE,32);
   IOLog("A04BR3 WMT: boot bundle reservation/mapping rejected; no fallback\n");
  }
  if(!stageError){
   IOLog("A04BR3 WMT: SDIO mandatory STP, cfgExist=%u antenna=%u co-clock=%u merge=%u FM=%u; starting verified patch set\n",startupOptions.coex_present,startupOptions.antenna_mode,startupOptions.co_clock,startupOptions.merge_interface,startupOptions.fm_strap);
   if(!startWMTFirmware())loadBootWLANFirmware(provider);
  }
 }
 /* Readiness stays separate: WCIR ready alone does not publish a network. */
 setProperty("WMTTransportReady",transportReady?kOSBooleanTrue:kOSBooleanFalse);
 setProperty("WMTCCIR",wmt.ccir,32);setProperty("WMTCHLPCR",wmt.chlpcr_after,32);
 setProperty("SDIOVendor",card.vendor,32);setProperty("SDIODevice",card.device,32);setProperty("NetworkDataPathAvailable",kOSBooleanFalse);registerService();return true;
}

extern "C" int a04br3_wmt_stage_firmware(IOService *service,const void *bytes,unsigned length)
{
 A04BR3SDIO *sdio=OSDynamicCast(A04BR3SDIO,service);
 return sdio?sdio->stageWMTFirmware(bytes,length):A04_WMT_FW_STATE;
}
extern "C" int a04br3_wmt_copy_firmware(IOService *service,unsigned sequence,unsigned offset,void *bytes,unsigned length)
{
 A04BR3SDIO *sdio=OSDynamicCast(A04BR3SDIO,service);
 return sdio?sdio->copyWMTFirmware(sequence,offset,bytes,length):A04_WMT_FW_STATE;
}

extern "C" int a04br3_wmt_start_firmware(IOService *service)
{
 A04BR3SDIO *sdio=OSDynamicCast(A04BR3SDIO,service);
 return sdio?sdio->startWMTFirmware():A04_WMT_DOWNLOAD_STATE;
}

extern "C" int a04br3_wlan_stage_firmware(IOService *service,const void *bytes,unsigned length)
{
 A04BR3SDIO *sdio=OSDynamicCast(A04BR3SDIO,service);
 return sdio?sdio->stageWLANFirmware(bytes,length):A04_WLAN_STATE;
}
extern "C" int a04br3_wlan_start_firmware(IOService *service)
{
 A04BR3SDIO *sdio=OSDynamicCast(A04BR3SDIO,service);
 return sdio?sdio->startWLANFirmware():A04_WLAN_STATE;
}
