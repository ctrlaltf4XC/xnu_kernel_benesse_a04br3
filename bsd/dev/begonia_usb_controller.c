/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board-only glue for source-built MUSB gadget + verified clock/PHY/PWRAP.
 * Singleton polled owner, no IRQ/DMA or host VBUS source. Explicit opt-in
 * is enforced before this adapter is called by begonia_usb_init().
 */
#if defined(ARM64_BOARD_CONFIG_BEGONIA)
#include <sys/systm.h>
#include <sys/errno.h>
#include <kern/clock.h>
#include <vm/memory_types.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include "begonia_pwrap_lock.h"
#include "begonia_usb_stream.h"
#include "../../iokit/Kernel/arm/usb/BEGONIAUSBDevice.h"
#include "../../iokit/Kernel/arm/usb/BEGONIAUSBPwrap.h"
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern uint64_t mach_absolute_time(void);
extern void absolutetime_to_nanoseconds(uint64_t,uint64_t *);
int begonia_usb_controller_start(void);
int begonia_usb_controller_poll(void);
void begonia_usb_controller_stop(void);

struct usb_board {vm_offset_t maps[4];unsigned fault,leased,prepared;uint64_t generation;};
static struct usb_board board;
static struct begonia_usb_device device;
static struct begonia_usb_platform_io platform;
static struct begonia_usb_pwrap_io pwrap;
static struct begonia_usb_platform_result observed;
static vm_offset_t region(void *ctx,unsigned r,unsigned off,unsigned width) {
 struct usb_board *b=ctx;unsigned span=r==BEGONIA_USB_PHY?0x800:0x1000;
 if(r>3 || !b->maps[r] || off>span-width || (off&(width-1))){b->fault=1;return 0;}
 return b->maps[r]+off;
}
static void barrier(void *ctx){(void)ctx;__asm__ volatile("dsb sy":::"memory");}
static uint8_t read8(void *ctx,unsigned r,unsigned off){vm_offset_t p=region(ctx,r,off,1);if(!p)return 0;uint8_t v=*(volatile uint8_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static uint32_t read32(void *ctx,unsigned r,unsigned off){vm_offset_t p=region(ctx,r,off,4);if(!p)return 0;uint32_t v=*(volatile uint32_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static void write8(void *ctx,unsigned r,unsigned off,uint8_t v){vm_offset_t p=region(ctx,r,off,1);if(p)*(volatile uint8_t *)p=v;barrier(ctx);}
static void write32(void *ctx,unsigned r,unsigned off,uint32_t v){vm_offset_t p=region(ctx,r,off,4);if(p)*(volatile uint32_t *)p=v;barrier(ctx);}
static uint8_t mac8(void *ctx,unsigned off){return read8(ctx,BEGONIA_USB_MAC,off);}
static uint16_t mac16(void *ctx,unsigned off){vm_offset_t p=region(ctx,BEGONIA_USB_MAC,off,2);if(!p)return 0;uint16_t v=*(volatile uint16_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static void macw8(void *ctx,unsigned off,uint8_t v){write8(ctx,BEGONIA_USB_MAC,off,v);}
static void macw16(void *ctx,unsigned off,uint16_t v){vm_offset_t p=region(ctx,BEGONIA_USB_MAC,off,2);if(p)*(volatile uint16_t *)p=v;barrier(ctx);}
static void wait_us(void *ctx,unsigned us){(void)ctx;if(us>20000){board.fault=1;return;}delay(us);}
static uint32_t pwread(void *ctx,unsigned off){return read32(ctx,3,off);}
static void pwwrite(void *ctx,unsigned off,uint32_t v){write32(ctx,3,off,v);}
static int pwlock(void *ctx){(void)ctx;return begonia_pwrap_try_lock();}
static void pwunlock(void *ctx){(void)ctx;begonia_pwrap_unlock();}
static int supply(void *ctx){(void)ctx;return begonia_usb_pwrap_vusb_enable(&pwrap);}
static int ready(void *ctx){(void)ctx;return begonia_usb_device_observe(&platform,&observed);}
static int push(void *ctx,const uint8_t *p,unsigned n){(void)ctx;return begonia_usb_console_rx(p,n)==n?0:EAGAIN;}
static unsigned pull(void *ctx,uint8_t *p,unsigned n){(void)ctx;return begonia_usb_console_tx(p,n);}
static void completed(void *ctx){(void)ctx;begonia_usb_console_tx_complete();}
static void disconnected(void *ctx){(void)ctx;begonia_usb_console_online(0);}
int begonia_usb_controller_start(void) {
 unsigned enable=0;
 if(!PE_parse_boot_argn("a04br3.usb",&enable,sizeof(enable)) || enable!=1)return EPERM;
 if(board.leased)return EBUSY;
 board.leased=1;
 board.maps[0]=ml_io_map(0x11100000,0x1000);
 /* Map full aligned PHY page; header's region begins at physical+0x800. */
 vm_offset_t phy=ml_io_map(0x11110000,0x1000);board.maps[1]=phy?phy+0x800:0;
 board.maps[2]=ml_io_map(0x10000000,0x1000);
 board.maps[3]=ml_io_map(0x1000f000,0x1000);
 for(unsigned i=0;i<4;i++)if(!board.maps[i])return ENOMEM;
 pwrap=(struct begonia_usb_pwrap_io){&board,0x1000f000,0x1000,pwread,pwwrite,wait_us,pwlock,pwunlock};
 platform=(struct begonia_usb_platform_io){&board,0x11100000,0x1000,0x11110800,0x800,0x10000000,0x1000,
  read8,write8,read32,write32,barrier,wait_us,supply};
 int e=begonia_usb_device_prepare(&platform,&observed);
 printf("BEGONIA USB: platform=%d stage=%u supply=%d devctl=0x%x phy=%x/%x\n",e,observed.stage,observed.supply_error,observed.devctl,observed.phy6c,observed.phy6d);
 if(board.fault)return EIO;
 if(e && e!=BEGONIA_USB_PLATFORM_NO_VBUS && !(e==BEGONIA_USB_PLATFORM_POWER && observed.supply_error==EBUSY))return EIO;
 board.prepared=(e==0 || e==BEGONIA_USB_PLATFORM_NO_VBUS);
 struct begonia_usb_console_ops ops={&board,push,pull,completed,disconnected};
 begonia_usb_device_bind_console(&device,&ops);
 board.generation=begonia_usb_console_generation();return 0;
}
int begonia_usb_controller_poll(void) {
 if(!board.leased || board.fault)return -EIO;
 if(!board.prepared){
  int e=begonia_usb_device_prepare(&platform,&observed);
  if(e==BEGONIA_USB_PLATFORM_POWER && observed.supply_error==EBUSY)return 0;
  if(e && e!=BEGONIA_USB_PLATFORM_NO_VBUS)return -EIO;
  board.prepared=1;
 }
 uint64_t generation=begonia_usb_console_generation();
 if(generation!=board.generation){
  /* New console owner cannot replay old commands or receive cached stdout. */
  begonia_usb_session_reset(&device.session);begonia_usb_console_reset(&device.console);board.generation=generation;
 }
 if(!device.active){
  int e=begonia_usb_device_observe(&platform,&observed);
  if(e==BEGONIA_USB_PLATFORM_NO_VBUS)return 0;
  if(e){printf("BEGONIA USB: observe=%d devctl=0x%x phy=%x/%x\n",e,observed.devctl,observed.phy6c,observed.phy6d);return -EIO;}
  struct begonia_musb_io io={&board,0x11100000,0x1000,64,ready,mac8,mac16,macw8,macw16,barrier};
  /* Explicit private development identity shared with the host selector.
   * This is not an Apple/stock updater or a production USB-ID assignment. */
  struct begonia_usb_device_policy policy={0x0e8d,0x20ff,0x80,50};
  if(begonia_usb_device_start(&device,1,&io,&policy,wait_us))return -EIO;
  printf("BEGONIA USB: peripheral softconnect; private console 0e8d:20ff\n");
 }
 unsigned before=device.configured;int e=begonia_usb_device_poll(&device,1);
 /* SET_ADDRESS/SET_CONFIGURATION take effect at the status-stage event; the
  * host uses the new address about 2ms later. Spin briefly (max 5ms). */
 for(unsigned spin=0;device.active && device.pending && e!=BEGONIA_MUSB_FAULT && spin<1000;spin++){
  wait_us(&board,5);e=begonia_usb_device_poll(&device,1);
 }
 /* Time-based fallback. Poll counts are not time: the worker batches up to
  * 16 polls per 1ms wake, so a count-based 3000-poll limit reconnected every
  * ~0.2s and aborted the ~2s HS reset/chirp (host: "Cannot enable"). Wait
  * 10s per attempt; two HS attempts, then full speed (HSENAB=0). */
 { static uint64_t since;static unsigned attempt;
   if(device.active && !device.configured){
    uint64_t now=mach_absolute_time(),ns=0;if(!since)since=now;
    absolutetime_to_nanoseconds(now-since,&ns);
    if(ns>=10000000000ULL && attempt<4){
     since=now;attempt++;uint8_t p=mac8(&board,1);
     macw8(&board,1,(uint8_t)(p&~0x40u));for(unsigned n=0;n<10;n++)wait_us(&board,20000);
     if(attempt>=2)p&=(uint8_t)~0x20u;
     macw8(&board,1,(uint8_t)(p|0x40u));
     printf("BEGONIA USB: not configured, reconnect attempt=%u power=0x%x hs=%u\n",attempt,mac8(&board,1),attempt<2);
    }
   } else since=0; }
 /* Bounded enumeration diagnostics: never read clear-on-read INTRUSB here. */
 { static unsigned ticks;
   if(device.active && !device.configured && (++ticks%400)==0 && ticks<=400*60)
    printf("BEGONIA USB DIAG: t=%u power=0x%x devctl=0x%x faddr=0x%x addr=%u pending=%u phy18=0x%x phy1a=0x%x phy68=0x%x phy6c=0x%x phy6d=0x%x setups=%u last=0x%x len=%u csr=0x%x ep0=%u resets=%u\n",
     ticks,mac8(&board,1),mac8(&board,0x60),mac8(&board,0),device.address,device.pending,
     read8(&board,BEGONIA_USB_PHY,0x18),read8(&board,BEGONIA_USB_PHY,0x1a),read32(&board,BEGONIA_USB_PHY,0x68),
     read8(&board,BEGONIA_USB_PHY,0x6c),read8(&board,BEGONIA_USB_PHY,0x6d),device.setups,device.last_setup,device.last_len,device.last_csr,device.ep0_events,device.resets); }
 if(board.fault)return -EIO;
 if(e==BEGONIA_MUSB_FAULT)return -EIO;
 if(e==BEGONIA_MUSB_NOT_READY){begonia_usb_console_online(0);return 0;}
 begonia_usb_console_online(device.active&&device.configured);
 if(before!=device.configured)printf("BEGONIA USB: configured=%u speed_mps=%u\n",device.configured,device.mps);
 /* While a host is enumerating, keep the worker at 1ms cadence. */
 if(device.active && !device.configured)return 1;
 return e==0?1:0;
}
void begonia_usb_controller_stop(void) {
 begonia_usb_console_online(0);begonia_usb_device_stop(&device);board.leased=0;
 /* Clock/PHY rails may be shared: never power them down speculatively. */
}
#endif
