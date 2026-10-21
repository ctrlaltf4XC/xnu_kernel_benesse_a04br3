/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original polling peripheral owner. Register facts: pinned board BSP
 * usb20/mtk_musb_reg.h, musb_gadget_ep0.c, musb_gadget.c (see sources.json).
 * Caller owns exclusive MAC/PHY lease and serializes calls. Platform prepare
 * must supply real VUSB/clock/device/VBUS before start. No VBUS drive or DMA.
 */
#ifndef A04BR3_USB_DEVICE_H
#define A04BR3_USB_DEVICE_H
#include "A04BR3MUSBPIO.h"
#include "A04BR3USBConsole.h"
struct a04_usb_device_policy {uint16_t vid,pid;uint8_t attributes,maxpower;};
struct a04_usb_device {struct a04_musb_io io;struct a04_usb_device_policy policy;
 struct a04_usb_session session;struct a04_usb_console console;
 unsigned active,configured,address,mps,pending,value,ep0_in,suspended,tx_pending;};
static inline void a04_usb_device_bar(struct a04_usb_device *s){s->io.barrier(s->io.context);}
static inline void a04_usb_device_index(struct a04_usb_device *s,unsigned e){s->io.write8(s->io.context,14,(uint8_t)e);a04_usb_device_bar(s);}
static inline void a04_usb_device_fifos(struct a04_usb_device *s){
 struct a04_musb_io *i=&s->io;void *c=i->context;a04_usb_device_index(s,1);
 i->write16(c,0x10,(uint16_t)s->mps);i->write16(c,0x14,(uint16_t)s->mps);
 i->write8(c,0x62,s->mps==512?6:3);i->write8(c,0x63,s->mps==512?6:3);
 i->write16(c,0x64,8);i->write16(c,0x66,(uint16_t)(8+s->mps/8));
 i->write16(c,0x12,0x2048);i->write16(c,0x16,0x90);a04_usb_device_index(s,0);
}
static inline void a04_usb_device_reset(struct a04_usb_device *s){
 s->configured=s->address=s->pending=s->ep0_in=s->suspended=s->tx_pending=0;
 if(s->console.ops.disconnected)s->console.ops.disconnected(s->console.ops.context);
 a04_usb_session_reset(&s->session);a04_usb_console_reset(&s->console);
 s->mps=(s->io.read8(s->io.context,1)&0x10)?512:64;
 s->io.write8(s->io.context,0,0);a04_usb_device_fifos(s);
}
static inline int a04_usb_device_start(struct a04_usb_device *s,unsigned enable,
 const struct a04_musb_io *io,const struct a04_usb_device_policy *p,
 void (*wait_us)(void *,uint32_t)){
 if(enable!=1)return A04_MUSB_DISABLED;
 if(!s || !io || !p || s->active || !p->vid || !p->pid ||
 (p->attributes!=0x80 && p->attributes!=0xc0) || p->maxpower>250 ||
 io->physical!=0x11100000 || io->span<0x1000 || !io->read8 || !io->read16 ||
 !io->write8 || !io->write16 || !io->barrier || !wait_us)return A04_MUSB_NOT_READY;
 unsigned d=io->read8(io->context,0x60);
 if((d&0x9e)!=0x98)return A04_MUSB_NOT_READY;
 s->io=*io;s->policy=*p;void *c=io->context;
 io->write8(c,1,(uint8_t)(io->read8(c,1)&~0x4dU));
 io->write16(c,6,0);io->write16(c,8,0);io->write8(c,11,0);
 for(unsigned n=0;n<8;n++)io->write16(c,0x204+16*n,0);
 a04_usb_device_bar(s);wait_us(c,20000);
 io->write16(c,0x74,(uint16_t)(io->read16(c,0x74)|3));
 a04_usb_device_reset(s);s->active=1;
 io->write8(c,1,(uint8_t)((io->read8(c,1)&~0x0dU)|0x60));a04_usb_device_bar(s);return 0;
}
static inline void a04_usb_device_stop(struct a04_usb_device *s){
 if(!s || !s->active)return;
 s->io.write8(s->io.context,1,(uint8_t)(s->io.read8(s->io.context,1)&~0x40U));
 a04_usb_device_reset(s);a04_usb_device_bar(s);s->active=0;
}
static inline void a04_usb_device_bind_console(struct a04_usb_device *s,const struct a04_usb_console_ops *o){s->console.ops=*o;}
static inline void a04_usb_device_stall(struct a04_usb_device *s){s->pending=s->ep0_in=0;s->io.write16(s->io.context,0x12,0x60);}
static inline void a04_usb_device_ep0(struct a04_usb_device *s,unsigned txevent){
 struct a04_musb_io *i=&s->io;void *c=i->context;a04_usb_device_index(s,0);
 unsigned csr=i->read16(c,0x12);
 if(csr&0x10){i->write16(c,0x12,0x80);s->pending=s->ep0_in=0;csr=i->read16(c,0x12);}
 if(csr&4){i->write16(c,0x12,0);s->pending=s->ep0_in=0;return;}
 /* Address/config side effects only after hardware status-IN completion. */
 if(s->pending && txevent && !(csr&0x0a)){
 if(s->pending==1){s->address=s->value;i->write8(c,0,(uint8_t)s->value);}
 else {s->configured=s->value;s->tx_pending=0;if(s->console.ops.disconnected)s->console.ops.disconnected(s->console.ops.context);a04_usb_device_fifos(s);a04_usb_session_reset(&s->session);a04_usb_console_reset(&s->console);}
 s->pending=0;
 }
 if(!(csr&1))return;
 unsigned count=i->read16(c,0x18);
 if(count==0 && s->ep0_in){i->write16(c,0x12,0x40);s->ep0_in=0;return;}
 if(count!=8){a04_usb_device_stall(s);return;}
 uint8_t q[8],out[64];for(unsigned b=0;b<8;b++)q[b]=i->read8(c,0x20);
 unsigned value=q[2]|((unsigned)q[3]<<8),index=q[4]|((unsigned)q[5]<<8),len=q[6]|((unsigned)q[7]<<8),n=0;
 s->pending=s->ep0_in=0;
 for(unsigned b=0;b<64;b++)out[b]=0;
 if(q[0]==0x80 && q[1]==6){
 unsigned type=value>>8,number=value&255;
 if(type==1 && !number && !index){uint8_t d[18]={18,1,0,2,0,0,0,64,0,0,0,0,0,1,0,2,0,1};
 d[8]=(uint8_t)s->policy.vid;d[9]=(uint8_t)(s->policy.vid>>8);d[10]=(uint8_t)s->policy.pid;d[11]=(uint8_t)(s->policy.pid>>8);
 for(unsigned b=0;b<18;b++)out[b]=d[b];n=18;}
 else if((type==2 || type==7) && !number && !index){unsigned mp=type==7?(s->mps==512?64:512):s->mps;
 uint8_t d[32]={9,2,32,0,1,1,0,0x80,0,9,4,0,0,2,255,0,1,0,7,5,1,2,0,0,0,7,5,0x81,2,0,0,0};
 d[1]=(uint8_t)type;d[7]=s->policy.attributes;d[8]=s->policy.maxpower;d[22]=d[29]=(uint8_t)mp;d[23]=d[30]=(uint8_t)(mp>>8);
 for(unsigned b=0;b<32;b++)out[b]=d[b];n=32;}
 else if(type==6 && !number && !index){uint8_t d[10]={10,6,0,2,0,0,0,64,1,0};for(unsigned b=0;b<10;b++)out[b]=d[b];n=10;}
 else if(type==3 && number==0 && !index){out[0]=4;out[1]=3;out[2]=9;out[3]=4;n=4;}
 else if(type==3 && number==2 && index==0x409){const char *name="A04BR3 XNU USB Console";n=2;while(*name && n+2<=64){out[n++]=(uint8_t)*name++;out[n++]=0;}out[0]=(uint8_t)n;out[1]=3;}
 else {a04_usb_device_stall(s);return;}
 }else if(q[0]==0 && q[1]==5 && !index && !len && value<=127 && !s->configured){s->pending=1;s->value=value;}
 else if(q[0]==0 && q[1]==9 && !index && !len && value<=1 && s->address){s->pending=2;s->value=value;}
 else if(q[0]==0x80 && q[1]==8 && !value && !index && len==1){out[0]=(uint8_t)s->configured;n=1;}
 else if(q[0]==0x80 && !q[1] && !value && !index && len==2){out[0]=(s->policy.attributes&0x40)?1:0;n=2;}
 else if(q[0]==0x81 && !q[1] && !value && !index && len==2 && s->configured){n=2;}
 else if(q[0]==0x82 && !q[1] && !value && len==2 &&
 (index==0 || index==0x80 || (s->configured && (index==1 || index==0x81)))){
 if(index==1 || index==0x81){a04_usb_device_index(s,1);out[0]=(i->read16(c,index==1?0x16:0x12)&(index==1?0x20:0x10))?1:0;a04_usb_device_index(s,0);}n=2;}
 else if(q[0]==0x81 && q[1]==10 && !value && !index && len==1 && s->configured){n=1;}
 else if(q[0]==1 && q[1]==11 && !value && !index && !len && s->configured){}
 else {a04_usb_device_stall(s);return;}
 if(n){if(n>len)n=len;for(unsigned b=0;b<n;b++)i->write8(c,0x20,out[b]);i->write16(c,0x12,0x4a);s->ep0_in=1;}
 else i->write16(c,0x12,0x48);
 a04_usb_device_bar(s);
}
static inline int a04_usb_device_bulk(struct a04_usb_device *s,unsigned txevent){
 if(!s->configured)return A04_MUSB_AGAIN;
 struct a04_musb_io *i=&s->io;void *c=i->context;a04_usb_device_index(s,1);
 unsigned tx=i->read16(c,0x12),rx=i->read16(c,0x16);
 if((tx&0xdcb4) || !(tx&0x2000) || (rx&0xe96c))return A04_MUSB_FAULT;
 if(s->tx_pending && txevent && !(tx&3)){s->tx_pending=0;if(s->console.ops.tx_complete)s->console.ops.tx_complete(s->console.ops.context);}
 /* ACK and SUSPEND can coalesce between finite worker polls. Retire the
  * already ACKed IN before gating new OUT work, or drain loses its event. */
 if(s->suspended || (tx&3) || !(rx&1))return A04_MUSB_AGAIN;
 unsigned n=i->read16(c,0x18),written=0;uint8_t in[512],out[512];
 if(n>s->mps)return A04_MUSB_FAULT;
 for(unsigned b=0;b<n;b++)in[b]=i->read8(c,0x24);
 int r=a04_usb_console_process(&s->session,&s->console,in,n,out,sizeof(out),s->mps,&written);
 i->write16(c,0x16,(uint16_t)((rx|0x44)&~1U));a04_usb_device_bar(s);
 if(r)return A04_MUSB_BAD_FRAME;
 for(unsigned b=0;b<written;b++)i->write8(c,0x24,out[b]);a04_usb_device_bar(s);
 s->tx_pending=1;i->write16(c,0x12,(uint16_t)(tx|0xa7));a04_usb_device_bar(s);return 0;
}
static inline int a04_usb_device_poll(struct a04_usb_device *s,unsigned budget){
 if(!s || !s->active)return A04_MUSB_DISABLED;
 if(!budget || budget>8)return A04_MUSB_NOT_READY;
 struct a04_musb_io *i=&s->io;void *c=i->context;unsigned old=i->read8(c,14);int result=A04_MUSB_AGAIN;
 for(unsigned b=0;b<budget;b++){
 if((i->read8(c,0x60)&0x9e)!=0x98){a04_usb_device_stop(s);return A04_MUSB_NOT_READY;}
 unsigned bus=i->read8(c,10),tx=i->read16(c,2),rx=i->read16(c,4);
 if(bus)i->write8(c,10,(uint8_t)bus);if(tx)i->write16(c,2,(uint16_t)tx);if(rx)i->write16(c,4,(uint16_t)rx);
 if(bus&0x24){a04_usb_device_reset(s);if(bus&0x20)continue;}
 if(bus&1)s->suspended=1;if(bus&2)s->suspended=0;
 a04_usb_device_ep0(s,tx&1);result=a04_usb_device_bulk(s,tx&2);
 if(result==A04_MUSB_FAULT){a04_usb_device_stop(s);return result;}
 }
 a04_usb_device_index(s,old);return result;
}
#endif
