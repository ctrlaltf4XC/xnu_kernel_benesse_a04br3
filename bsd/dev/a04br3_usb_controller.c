/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board-only glue for source-built MUSB gadget + verified clock/PHY/PWRAP.
 * Singleton polled owner, no IRQ/DMA or host VBUS source. Explicit opt-in
 * is enforced before this adapter is called by a04br3_usb_init().
 */
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#include <sys/systm.h>
#include <sys/errno.h>
#include <kern/clock.h>
#include <vm/memory_types.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include "a04br3_pwrap_lock.h"
#include "a04br3_usb_stream.h"
#include "../../iokit/Kernel/arm/usb/A04BR3USBDevice.h"
#include "../../iokit/Kernel/arm/usb/A04BR3USBPwrap.h"
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
int a04br3_usb_controller_start(void);
int a04br3_usb_controller_poll(void);
void a04br3_usb_controller_stop(void);

struct usb_board {vm_offset_t maps[4];unsigned fault,leased,prepared;uint64_t generation;};
static struct usb_board board;
static struct a04_usb_device device;
static struct a04_usb_platform_io platform;
static struct a04_usb_pwrap_io pwrap;
static struct a04_usb_platform_result observed;
static vm_offset_t region(void *ctx,unsigned r,unsigned off,unsigned width) {
 struct usb_board *b=ctx;unsigned span=r==A04_USB_PHY?0x800:0x1000;
 if(r>3 || !b->maps[r] || off>span-width || (off&(width-1))){b->fault=1;return 0;}
 return b->maps[r]+off;
}
static void barrier(void *ctx){(void)ctx;__asm__ volatile("dsb sy":::"memory");}
static uint8_t read8(void *ctx,unsigned r,unsigned off){vm_offset_t p=region(ctx,r,off,1);if(!p)return 0;uint8_t v=*(volatile uint8_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static uint32_t read32(void *ctx,unsigned r,unsigned off){vm_offset_t p=region(ctx,r,off,4);if(!p)return 0;uint32_t v=*(volatile uint32_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static void write8(void *ctx,unsigned r,unsigned off,uint8_t v){vm_offset_t p=region(ctx,r,off,1);if(p)*(volatile uint8_t *)p=v;barrier(ctx);}
static void write32(void *ctx,unsigned r,unsigned off,uint32_t v){vm_offset_t p=region(ctx,r,off,4);if(p)*(volatile uint32_t *)p=v;barrier(ctx);}
static uint8_t mac8(void *ctx,unsigned off){return read8(ctx,A04_USB_MAC,off);}
static uint16_t mac16(void *ctx,unsigned off){vm_offset_t p=region(ctx,A04_USB_MAC,off,2);if(!p)return 0;uint16_t v=*(volatile uint16_t *)p;__asm__ volatile("dmb oshld":::"memory");return v;}
static void macw8(void *ctx,unsigned off,uint8_t v){write8(ctx,A04_USB_MAC,off,v);}
static void macw16(void *ctx,unsigned off,uint16_t v){vm_offset_t p=region(ctx,A04_USB_MAC,off,2);if(p)*(volatile uint16_t *)p=v;barrier(ctx);}
static void wait_us(void *ctx,unsigned us){(void)ctx;if(us>20000){board.fault=1;return;}delay(us);}
static uint32_t pwread(void *ctx,unsigned off){return read32(ctx,3,off);}
static void pwwrite(void *ctx,unsigned off,uint32_t v){write32(ctx,3,off,v);}
static int pwlock(void *ctx){(void)ctx;return a04br3_pwrap_try_lock();}
static void pwunlock(void *ctx){(void)ctx;a04br3_pwrap_unlock();}
static int supply(void *ctx){(void)ctx;return a04_usb_pwrap_vusb_enable(&pwrap);}
static int ready(void *ctx){(void)ctx;return a04_usb_device_observe(&platform,&observed);}
static int push(void *ctx,const uint8_t *p,unsigned n){(void)ctx;return a04br3_usb_console_rx(p,n)==n?0:EAGAIN;}
static unsigned pull(void *ctx,uint8_t *p,unsigned n){(void)ctx;return a04br3_usb_console_tx(p,n);}
static void completed(void *ctx){(void)ctx;a04br3_usb_console_tx_complete();}
static void disconnected(void *ctx){(void)ctx;a04br3_usb_console_online(0);}
int a04br3_usb_controller_start(void) {
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
 pwrap=(struct a04_usb_pwrap_io){&board,0x1000f000,0x1000,pwread,pwwrite,wait_us,pwlock,pwunlock};
 platform=(struct a04_usb_platform_io){&board,0x11100000,0x1000,0x11110800,0x800,0x10000000,0x1000,
  read8,write8,read32,write32,barrier,wait_us,supply};
 int e=a04_usb_device_prepare(&platform,&observed);
 printf("A04BR3 USB: platform=%d stage=%u supply=%d devctl=0x%x phy=%x/%x\n",e,observed.stage,observed.supply_error,observed.devctl,observed.phy6c,observed.phy6d);
 if(board.fault)return EIO;
 if(e && e!=A04_USB_PLATFORM_NO_VBUS && !(e==A04_USB_PLATFORM_POWER && observed.supply_error==EBUSY))return EIO;
 board.prepared=(e==0 || e==A04_USB_PLATFORM_NO_VBUS);
 struct a04_usb_console_ops ops={&board,push,pull,completed,disconnected};
 a04_usb_device_bind_console(&device,&ops);
 board.generation=a04br3_usb_console_generation();return 0;
}
int a04br3_usb_controller_poll(void) {
 if(!board.leased || board.fault)return -EIO;
 if(!board.prepared){
  int e=a04_usb_device_prepare(&platform,&observed);
  if(e==A04_USB_PLATFORM_POWER && observed.supply_error==EBUSY)return 0;
  if(e && e!=A04_USB_PLATFORM_NO_VBUS)return -EIO;
  board.prepared=1;
 }
 uint64_t generation=a04br3_usb_console_generation();
 if(generation!=board.generation){
  /* New console owner cannot replay old commands or receive cached stdout. */
  a04_usb_session_reset(&device.session);a04_usb_console_reset(&device.console);board.generation=generation;
 }
 if(!device.active){
  int e=a04_usb_device_observe(&platform,&observed);
  if(e==A04_USB_PLATFORM_NO_VBUS)return 0;
  if(e){printf("A04BR3 USB: observe=%d devctl=0x%x phy=%x/%x\n",e,observed.devctl,observed.phy6c,observed.phy6d);return -EIO;}
  struct a04_musb_io io={&board,0x11100000,0x1000,64,ready,mac8,mac16,macw8,macw16,barrier};
  /* Explicit private development identity shared with the host selector.
   * This is not an Apple/stock updater or a production USB-ID assignment. */
  struct a04_usb_device_policy policy={0x0e8d,0x20ff,0x80,50};
  if(a04_usb_device_start(&device,1,&io,&policy,wait_us))return -EIO;
  printf("A04BR3 USB: peripheral softconnect; private console 0e8d:20ff\n");
 }
 unsigned before=device.configured;int e=a04_usb_device_poll(&device,1);
 /* Bounded fallbacks when no host address arrives: reconnect, then FS-only. */
 { static unsigned idle,attempt;
   if(device.active && !device.address && !device.configured){
    if(++idle>=2000 && attempt<6){
     idle=0;attempt++;uint8_t p=mac8(&board,1);
     macw8(&board,1,(uint8_t)(p&~0x40u));for(unsigned n=0;n<10;n++)wait_us(&board,20000);
     if(attempt>=3)p&=(uint8_t)~0x20u;
     macw8(&board,1,(uint8_t)(p|0x40u));
     printf("A04BR3 USB: no host address, reconnect attempt=%u power=0x%x hs=%u\n",attempt,mac8(&board,1),attempt<3);
    }
   } else idle=0; }
 /* Bounded enumeration diagnostics: never read clear-on-read INTRUSB here. */
 { static unsigned ticks;
   if(device.active && !device.configured && (++ticks%400)==0 && ticks<=400*60)
    printf("A04BR3 USB DIAG: t=%u power=0x%x devctl=0x%x faddr=0x%x addr=%u pending=%u phy18=0x%x phy1a=0x%x phy68=0x%x phy6c=0x%x phy6d=0x%x\n",
     ticks,mac8(&board,1),mac8(&board,0x60),mac8(&board,0),device.address,device.pending,
     read8(&board,A04_USB_PHY,0x18),read8(&board,A04_USB_PHY,0x1a),read32(&board,A04_USB_PHY,0x68),
     read8(&board,A04_USB_PHY,0x6c),read8(&board,A04_USB_PHY,0x6d)); }
 if(board.fault)return -EIO;
 if(e==A04_MUSB_FAULT)return -EIO;
 if(e==A04_MUSB_NOT_READY){a04br3_usb_console_online(0);return 0;}
 a04br3_usb_console_online(device.active&&device.configured);
 if(before!=device.configured)printf("A04BR3 USB: configured=%u speed_mps=%u\n",device.configured,device.mps);
 return e==0?1:0;
}
void a04br3_usb_controller_stop(void) {
 a04br3_usb_console_online(0);a04_usb_device_stop(&device);board.leased=0;
 /* Clock/PHY rails may be shared: never power them down speculatively. */
}
#endif
