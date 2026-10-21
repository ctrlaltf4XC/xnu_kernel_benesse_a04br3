/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 MSDC2/MT6630 bring-up, based on board register definitions.
 * Deliberately no IO80211 service or connected state until data path exists.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
#include "A04BR3SDIOProtocol.h"
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
class A04BR3SDIO : public IOService {
 OSDeclareDefaultStructors(A04BR3SDIO);
 vm_offset_t host,top,gpio;
 static uint32_t rd(vm_offset_t b,unsigned o){uint32_t v=*(volatile uint32_t *)(b+o);__asm__ volatile("dmb oshld":::"memory");return v;}
 static void wr(vm_offset_t b,unsigned o,uint32_t v){*(volatile uint32_t *)(b+o)=v;__asm__ volatile("dsb oshst":::"memory");}
 static bool wait(vm_offset_t b,unsigned o,uint32_t mask,uint32_t value){for(unsigned n=0;n<10000;n++){if((rd(b,o)&mask)==value)return true;IODelay(10);}return false;}
 void pin(unsigned p,unsigned mode){unsigned o=0x300+(p/5)*16,s=(p%5)*3;wr(gpio,o,(rd(gpio,o)&~(7u<<s))|(mode<<s));}
 void output(unsigned p,bool high){pin(p,0);wr(gpio,(high?0x104:0x108)+(p/32)*16,1u<<(p%32));wr(gpio,4+(p/32)*16,1u<<(p%32));}
 static int command(void *p,unsigned op,uint32_t arg,unsigned response,uint32_t *out){
  A04BR3SDIO *s=(A04BR3SDIO *)p;
  if(!wait(s->host,0x3c,1,0))return 1;
  wr(s->host,0x0c,rd(s->host,0x0c));wr(s->host,0x38,arg);wr(s->host,0x34,op|(response<<7));
  for(unsigned n=0;n<10000;n++){
   uint32_t st=rd(s->host,0x0c)&0x700;
   if(st){wr(s->host,0x0c,st);if(st&0x600)return 2;*out=rd(s->host,0x40);if(response==7&&!wait(s->host,0x3c,1,0))return 3;return 0;}
   IODelay(10);
  }
  return 4;
 }
 static void delay(void *,unsigned us){IOSleep((us+999)/1000);}
public:bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(A04BR3SDIO,IOService);
bool A04BR3SDIO::start(IOService *provider)
{
 unsigned enabled=0;
 if(!PE_parse_boot_argn("a04br3.display_probe",&enabled,sizeof(enabled))||enabled!=1||!IOService::start(provider))return false;
 top=ml_io_map(0x10000000,0x1000);gpio=ml_io_map(0x10005000,0x1000);if(!top||!gpio)return false;
 /* Select 26MHz with source gated. TOP2 bit30 is an inverted gate. */
 wr(top,0xa0,2);wr(top,0x40,rd(top,0x40)&~(7u<<3));wr(top,0x9c,1);wr(top,0x6c,1u<<30);wr(top,0xb0,2);
 host=ml_io_map(0x11170000,0x1000);if(!host)return false;
 IOLog("A04BR3 SDIO: MSDC2 inherited cfg=0x%x GPIO=0x%x/0x%x\n",rd(host,0),rd(gpio,0x100),rd(gpio,0x200));
 /* External combo PMU GPIO4 and reset GPIO7; retain board RTC source.
  * Do not touch UART or the separate eMMC controller. */
 output(4,false);output(7,false);IOSleep(10);output(4,true);IOSleep(30);output(7,true);IOSleep(30);
 for(unsigned p=68;p<=73;p++)pin(p,1);
 wr(gpio,0x934,0x7e); /* MSDC2 input enables; board IES offsets are +4 set. */
 wr(host,0x10,0);wr(host,0,rd(host,0)|4);
 if(!wait(host,0,4,0)){IOLog("A04BR3 SDIO: reset timeout\n");return false;}
 wr(host,0,(rd(host,0)&~((3u<<20)|(0xfffu<<8)|2u))|9u|(17u<<8));
 if(!wait(host,0,0x80,0x80)){IOLog("A04BR3 SDIO: clock timeout cfg=0x%x\n",rd(host,0));return false;}
 wr(host,0x30,(rd(host,0x30)&~(3u<<16))|(1u<<19)|(0xffu<<24));
 IOSleep(10);
 struct a04_sdio_io io={this,command,delay};struct a04_sdio_card card={};int error=a04_sdio_identify(&io,&card);
 IOLog("A04BR3 SDIO: identify=%d OCR=0x%x RCA=0x%x functions=%u CCCR=0x%x vendor=0x%x device=0x%x\n",error,card.ocr,card.rca,card.functions,card.revision,card.vendor,card.device);
 if(error)return false;
 setProperty("SDIOVendor",card.vendor,32);setProperty("SDIODevice",card.device,32);setProperty("NetworkDataPathAvailable",kOSBooleanFalse);registerService();return true;
}
