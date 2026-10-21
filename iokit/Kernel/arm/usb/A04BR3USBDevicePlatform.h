/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Independent MT8167 peripheral clock/PHY register sequencing from board BSP
 * and EDK2 facts; source/hash provenance in USBDevicePlatform.sources.json.
 * Caller owns exclusive MAC/PHY/TOP lease and serialization. No IRQ, endpoint,
 * FIFO, GPIO/VBUS source, PLL/mux, SOFTCONN or SESSION enable is performed.
 * Success is observed register consistency, NOT enumeration/physical success.
 */
#ifndef A04BR3_USB_DEVICE_PLATFORM_H
#define A04BR3_USB_DEVICE_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
enum { A04_USB_MAC=0,A04_USB_PHY=1,A04_USB_TOP=2 };
enum { A04_USB_PLATFORM_INVALID=1,A04_USB_PLATFORM_POWER=2,
 A04_USB_PLATFORM_CLOCK=3,A04_USB_PLATFORM_PHY=4,A04_USB_PLATFORM_ROLE=5,
 A04_USB_PLATFORM_NO_VBUS=6 };
struct a04_usb_platform_io {
 void *context;
 uint32_t mac_physical,mac_span,phy_physical,phy_span,top_physical,top_span;
 uint8_t (*read8)(void *,unsigned,unsigned);
 void (*write8)(void *,unsigned,unsigned,uint8_t);
 uint32_t (*read32)(void *,unsigned,unsigned);
 void (*write32)(void *,unsigned,unsigned,uint32_t);
 void (*barrier)(void *);
 void (*delay_us)(void *,unsigned);
 /* Actual PMIC supply enable + hardware readback. 0 ONLY on verified supply
  * enable AND DA_QI status bits, not a cached property. Caller must serialize PWRAP transactions.
  * VUSB PHY rail != host VBUS 5V. No fallback when supply access is unavailable. */
 int (*vusb_enable_verified)(void *);
};
struct a04_usb_platform_result {
 uint32_t top1,top2;uint8_t devctl,phy6c,phy6d;
 unsigned stage;int supply_error;
};
/* A narrow adapter for an existing real serialized PMIC read/write transport.
 * PMIC identity must already have been established as this board MT6392.
 * No guessed regulator voltage/selector or host charger/drive setting. */
struct a04_usb_vusb_io {
 void *context;
 int (*read16)(void *,unsigned,uint16_t *);
 int (*write16)(void *,unsigned,uint16_t);
 void (*delay_us)(void *,unsigned);
};
static inline int a04_usb_vusb_enable(const struct a04_usb_vusb_io *p){
 if(!p||!p->read16||!p->write16||!p->delay_us)return A04_USB_PLATFORM_INVALID;
 uint16_t before=0,after=0;
 int error=p->read16(p->context,0x502,&before);if(error)return error;
 if(!(before&0x4000)){
  error=p->write16(p->context,0x502,(uint16_t)(before|0x4000));if(error)return error;
  p->delay_us(p->context,300);
 }
 error=p->read16(p->context,0x502,&after);if(error)return error;
 /* BSP regulator get_status queries DA_QI bit15, distinct from EN bit14. */
 return (after&0xc000)==0xc000?0:A04_USB_PLATFORM_POWER;
}
static inline int a04_usb_platform_valid(const struct a04_usb_platform_io *io){
 return io && io->mac_physical==0x11100000 && io->mac_span>=0x1000 &&
  io->phy_physical==0x11110800 && io->phy_span>=0x800 &&
  io->top_physical==0x10000000 && io->top_span>=0x1000 &&
  io->read8 && io->write8 && io->read32 && io->write32 && io->barrier &&
  io->delay_us && io->vusb_enable_verified;
}
static inline void a04_usb_platform_rmw8(const struct a04_usb_platform_io *io,
 unsigned region,unsigned off,unsigned clear,unsigned set){
 uint8_t before=io->read8(io->context,region,off);
 io->write8(io->context,region,off,(uint8_t)((before&~clear)|set));io->barrier(io->context);
}
/* Read-only and no spinning: revalidate each time before endpoint activation.
 * DTM1: only FORCE_IDDIG is enabled; no forced VBUS/AVALID/BVALID/SESSEND.
 * DEVCTL VBUS=3 must come from real comparator; zero means unplugged, not ready.
 */
static inline int a04_usb_device_observe(const struct a04_usb_platform_io *io,
 struct a04_usb_platform_result *r){
 if(!a04_usb_platform_valid(io)||!r)return A04_USB_PLATFORM_INVALID;
 r->stage=4;r->top1=io->read32(io->context,A04_USB_TOP,0x24);
 r->top2=io->read32(io->context,A04_USB_TOP,0x3c);
 r->devctl=io->read8(io->context,A04_USB_MAC,0x60);
 r->phy6c=io->read8(io->context,A04_USB_PHY,0x6c);
 r->phy6d=io->read8(io->context,A04_USB_PHY,0x6d);
 if((r->top1&(1u<<13))||(r->top2&((1u<<14)|(1u<<24)|(1u<<31))))return A04_USB_PLATFORM_CLOCK;
 if((r->phy6c&0x3f)!=2||(r->phy6d&0x3f)!=2)return A04_USB_PLATFORM_PHY;
 if(io->read32(io->context,A04_USB_PHY,0x68)&0x00ba3cf4u)return A04_USB_PLATFORM_PHY;
 if((r->devctl&0x87)!=0x80)return A04_USB_PLATFORM_ROLE; /*Bdevice, no HM/hostreq/session*/
 if((r->devctl&0x18)!=0x18)return A04_USB_PLATFORM_NO_VBUS;
 return 0;
}
static inline int a04_usb_device_prepare(const struct a04_usb_platform_io *io,
 struct a04_usb_platform_result *r){
 if(!a04_usb_platform_valid(io)||!r)return A04_USB_PLATFORM_INVALID;
 *r=(struct a04_usb_platform_result){0,0,0,0,0,1,0};
 r->supply_error=io->vusb_enable_verified(io->context);
 if(r->supply_error)return A04_USB_PLATFORM_POWER;
 r->stage=2;
 /* W1-clear aliases, never RMW TOP status or alter PLL/mux selection. */
 io->write32(io->context,A04_USB_TOP,0x9c,(1u<<14)|(1u<<24)|(1u<<31));
 io->write32(io->context,A04_USB_TOP,0x84,1u<<13);io->barrier(io->context);
 r->top1=io->read32(io->context,A04_USB_TOP,0x24);r->top2=io->read32(io->context,A04_USB_TOP,0x3c);
 if((r->top1&(1u<<13))||(r->top2&((1u<<14)|(1u<<24)|(1u<<31))))return A04_USB_PLATFORM_CLOCK;
 /* Keep disconnected while platform work runs. No host session request. */
 a04_usb_platform_rmw8(io,A04_USB_MAC,1,0x4c,0);
 a04_usb_platform_rmw8(io,A04_USB_MAC,0x60,3,0);
 io->delay_us(io->context,50);r->stage=3;
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x6b,4,0);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x6e,1,0);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x21,3,0);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x1a,0x80,0x10);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x22,3,4);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x6a,4,0);
 uint32_t dtm0=io->read32(io->context,A04_USB_PHY,0x68);
 io->write32(io->context,A04_USB_PHY,0x68,dtm0&~0x00ba3cf4u);io->barrier(io->context);
 a04_usb_platform_rmw8(io,A04_USB_PHY,0x18,8,6);
 io->delay_us(io->context,800);
 /* Source register definitions separate role request from sensed bus power.
  * Only IDDIG high is forced (peripheral); clear all bus-valid overrides. */
 uint32_t dtm1=io->read32(io->context,A04_USB_PHY,0x6c);
 io->write32(io->context,A04_USB_PHY,0x6c,(dtm1&~0x3f3fu)|0x202u);io->barrier(io->context);
 return a04_usb_device_observe(io,r);
}
#endif
