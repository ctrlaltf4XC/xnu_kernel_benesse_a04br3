/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original opt-in PIO leaf for verified MT8167 MUSB EP1 bulk IN/OUT.
 * Register facts: local m1.11 BSP usb20/mtk_musb_reg.h + musb_gadget.c;
 * actual board EDK2 Mt8167UsbHostDxe confirmsMAC11100000/PHY11110800.
 * No copied Linux implementation. No constructor/initcall/startup registration.
 * NOT a complete gadget: platform owner must implement verified clocks/PHY,
 * device role/VBUS, EP0 enumeration/configuration/FIFOs and exclusive lease.
 * ready() must revalidate that real lease/configuration under owner's lock.
 * Caller serializes start/poll/stop and resets session on bus reset/disconnect.
 * This leaf NEVER writes POWER/DEVCTL/PHY/clocks/IRQ/FIFO configuration.
 */
#ifndef A04BR3_MUSB_PIO_H
#define A04BR3_MUSB_PIO_H
#include "A04BR3USBBulk.h"
struct a04_musb_io {
 void *context;uint32_t physical,span;unsigned maxpacket;
 int (*ready)(void *); /*0 only for exclusive configured vendor EP1; never a synthetic readiness property*/
 uint8_t (*read8)(void *,unsigned);uint16_t (*read16)(void *,unsigned);
 void (*write8)(void *,unsigned,uint8_t);void (*write16)(void *,unsigned,uint16_t);
 void (*barrier)(void *);
};
struct a04_musb_pio {struct a04_musb_io io;struct a04_usb_session session;unsigned active;};
enum {A04_MUSB_DISABLED=1,A04_MUSB_NOT_READY=2,A04_MUSB_AGAIN=3,A04_MUSB_FAULT=4,A04_MUSB_BAD_FRAME=5};
static inline void a04_musb_stop(struct a04_musb_pio *s){s->active=0;a04_usb_session_reset(&s->session);}
static inline int a04_musb_role(const struct a04_musb_io *io){
 unsigned d=io->read8(io->context,0x60),p=io->read8(io->context,1);
 return (d&0x9c)==0x98 && (!!(p&0x10)==(io->maxpacket==512));
}
static inline int a04_musb_start(struct a04_musb_pio *s,unsigned enable,const struct a04_musb_io *io){
 if(enable!=1)return A04_MUSB_DISABLED; /* no callbacks/MMIO, even withouts/io */
 if(!s || !io || s->active || io->physical!=0x11100000 || io->span<0x1000 ||
  (io->maxpacket!=64 && io->maxpacket!=512) || !io->ready || !io->read8 || !io->read16 ||
  !io->write8 || !io->write16 || !io->barrier)return A04_MUSB_NOT_READY;
 if(io->ready(io->context) || !a04_musb_role(io))return A04_MUSB_NOT_READY;
 uint8_t index=io->read8(io->context,0xe);io->write8(io->context,0xe,1);io->barrier(io->context);
 unsigned tx=io->read16(io->context,0x10),rx=io->read16(io->context,0x14);
 io->write8(io->context,0xe,index);io->barrier(io->context);
 if(tx!=io->maxpacket || rx!=io->maxpacket)return A04_MUSB_NOT_READY;
 s->io=*io;a04_usb_session_reset(&s->session);s->active=1;return 0;
}
/* One packet per call; no spin, IRQ handling, DMA or arbitrary memory I/O. */
static inline int a04_musb_poll(struct a04_musb_pio *s){
 if(!s || !s->active)return A04_MUSB_DISABLED;
 const struct a04_musb_io *io=&s->io;void *c=io->context;
 if(io->ready(c) || !a04_musb_role(io)){a04_musb_stop(s);return A04_MUSB_NOT_READY;}
 uint8_t index=io->read8(c,0xe);io->write8(c,0xe,1);io->barrier(c);
 unsigned tx=io->read16(c,0x12),rx=io->read16(c,0x16);int result=A04_MUSB_AGAIN;
 /* Reject DMA/autoclear/iso/stall/error ownership, don't override it. */
 if((tx&0xdcb4) || !(tx&0x2000) || (rx&0xe96c)){result=A04_MUSB_FAULT;goto done;}
 if((tx&3) || !(rx&1))goto done; // backpressure: leave RX unconsumed
 {
  unsigned n=io->read16(c,0x18);uint8_t in[512],out[64];
  if(n>io->maxpacket){result=A04_MUSB_FAULT;goto done;}
  for(unsigned b=0;b<n;b++)in[b]=io->read8(c,0x24);
  result=a04_usb_bulk_process(&s->session,in,n,out,sizeof(out),io->maxpacket)?A04_MUSB_BAD_FRAME:0;
  /* WZC sentstall/overrun are preserved; clear only consumed RXPKTRDY. */
  io->write16(c,0x16,(uint16_t)((rx|0x44)&~1U));io->barrier(c);
  if(!result){for(unsigned b=0;b<64;b++)io->write8(c,0x24,out[b]);
   io->barrier(c);io->write16(c,0x12,(uint16_t)(tx|0xa7));io->barrier(c);}
 }
 done:io->write8(c,0xe,index);io->barrier(c);
 if(result==A04_MUSB_FAULT)a04_musb_stop(s);return result;
}
#endif
