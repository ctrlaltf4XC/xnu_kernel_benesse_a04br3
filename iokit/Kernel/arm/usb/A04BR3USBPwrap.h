/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded MT8167 WACS2 transport adapter for MT6392 VUSB only. Caller passes
 * the SAME board a04br3_pwrap_try_lock/unlock used by display and touch.
 * No recovery/clearing of another client's unfinished WACS response.
 */
#ifndef A04BR3_USB_PWRAP_H
#define A04BR3_USB_PWRAP_H
#include "A04BR3USBDevicePlatform.h"
struct a04_usb_pwrap_io {
 void *context;uint32_t physical,span;
 uint32_t (*read32)(void *,unsigned);
 void (*write32)(void *,unsigned,uint32_t);
 void (*delay_us)(void *,unsigned);
 int (*try_lock)(void *); /*1 acquired,0busy; shared board lock, NOT USB-only*/
 void (*unlock)(void *);
};
static inline int a04_usb_pw_valid(const struct a04_usb_pwrap_io *p){
 return p&&p->physical==0x1000f000&&p->span>=0x1000&&p->read32&&p->write32&&p->delay_us&&p->try_lock&&p->unlock;
}
/* Bounded10ms per hardware phase, no sleep/unbounded mutex acquisition.
 * Codes are Darwin errno:22EINVAL,16EBUSY,6ENXIO,60ETIMEDOUT. */
static inline int a04_usb_pw_wait(const struct a04_usb_pwrap_io *p,unsigned fsm,uint32_t *out){
 for(unsigned i=0;i<1000;i++){
  uint32_t v=p->read32(p->context,0xa4);
  if(((v>>16)&7)==fsm){if(out)*out=v;return 0;}
  p->delay_us(p->context,10);
 }
 return 60;
}
static inline int a04_usb_pwrap_read16(void *context,unsigned address,uint16_t *value){
 const struct a04_usb_pwrap_io *p=(const struct a04_usb_pwrap_io *)context;
 if(!a04_usb_pw_valid(p)||address!=0x502||!value)return 22;
 if(!p->try_lock(p->context))return 16;
 int error=0;uint32_t data=0;
 if(p->read32(p->context,0x9c)!=1){error=6;goto out;}
 error=a04_usb_pw_wait(p,0,0);if(error)goto out;
 p->write32(p->context,0xa0,(address>>1)<<16);
 error=a04_usb_pw_wait(p,6,&data);if(error)goto out;
 p->write32(p->context,0xa8,1);
 error=a04_usb_pw_wait(p,0,0);
 if(!error)*value=(uint16_t)data;
 out:p->unlock(p->context);return error;
}
static inline int a04_usb_pwrap_write16(void *context,unsigned address,uint16_t value){
 const struct a04_usb_pwrap_io *p=(const struct a04_usb_pwrap_io *)context;
 if(!a04_usb_pw_valid(p)||address!=0x502)return 22;
 if(!p->try_lock(p->context))return 16;
 int error=0;
 if(p->read32(p->context,0x9c)!=1){error=6;goto out;}
 error=a04_usb_pw_wait(p,0,0);if(error)goto out;
 p->write32(p->context,0xa0,0x80000000u|((address>>1)<<16)|value);
 error=a04_usb_pw_wait(p,0,0);
 out:p->unlock(p->context);return error;
}
static inline void a04_usb_pwrap_delay(void *context,unsigned us){
 const struct a04_usb_pwrap_io *p=(const struct a04_usb_pwrap_io *)context;
 p->delay_us(p->context,us);
}
static inline int a04_usb_pwrap_vusb_enable(void *context){
 struct a04_usb_vusb_io supply={context,a04_usb_pwrap_read16,a04_usb_pwrap_write16,a04_usb_pwrap_delay};
 return a04_usb_vusb_enable(&supply);
}
#endif
