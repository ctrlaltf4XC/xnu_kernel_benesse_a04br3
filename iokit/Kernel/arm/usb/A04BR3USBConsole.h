/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original bounded console adapter; existing console owns all command semantics.
 * Serialized caller. Callback push is all-or-nothing; pull must never exceed cap.
 * Exact last successful request replay is side-effect free, including TX drain.
 */
#ifndef A04BR3_USB_CONSOLE_H
#define A04BR3_USB_CONSOLE_H
#include "A04BR3USBBulk.h"
enum {A04_USB_CONSOLE_RX=7,A04_USB_CONSOLE_TX=8,A04_USB_BUSY=7};
struct a04_usb_console_ops {void *context;int (*push_rx)(void *,const uint8_t *,unsigned);unsigned (*pull_tx)(void *,uint8_t *,unsigned);void (*tx_complete)(void *);void (*disconnected)(void *);};
struct a04_usb_console {struct a04_usb_console_ops ops;unsigned request_bytes,reply_bytes;uint8_t request[512],reply[512];};
static inline void a04_usb_console_reset(struct a04_usb_console *c){c->request_bytes=c->reply_bytes=0;}
static inline int a04_usb_console_process(struct a04_usb_session *s,struct a04_usb_console *c,
 const void *request,size_t bytes,void *reply,size_t capacity,unsigned mps,unsigned *written){
 uint8_t checked[64];
 if((mps!=64 && mps!=512) || !s || !c || !written || capacity<mps || !a04_usb_span(reply,mps) ||
 !a04_usb_span(request,bytes) || !a04_usb_span(c,sizeof(*c)) ||
 a04_usb_overlap(c,sizeof(*c),request,bytes) || a04_usb_overlap(c,sizeof(*c),reply,mps) ||
 a04_usb_overlap(request,bytes,reply,mps) ||
 !a04_usb_span(s,sizeof(*s)) || a04_usb_overlap(s,sizeof(*s),reply,mps) ||
 a04_usb_overlap(s,sizeof(*s),c,sizeof(*c)))return A04_USB_BAD_FRAME;
 *written=0;
 /* Bulk validator verifies every length and CRC before accessing opcode. */
 int r=a04_usb_bulk_process(s,request,bytes,checked,sizeof(checked),mps);
 if(r)return r;
 const uint8_t *p=(const uint8_t *)request;uint8_t *o=(uint8_t *)reply;
 if(p[6]!=7 && p[6]!=8){for(unsigned i=0;i<64;i++)o[i]=checked[i];*written=64;
 a04_usb_console_reset(c);return 0;}
 unsigned status=a04_usb_u32(checked+32),n=(unsigned)bytes-32,count=0,outn=0;
 if(status==A04_USB_BAD_SEQUENCE && c->request_bytes==bytes && c->reply_bytes<=mps){
 unsigned same=1;for(unsigned i=0;i<bytes;i++)if(p[i]!=c->request[i])same=0;
 if(same){for(unsigned i=0;i<c->reply_bytes;i++)o[i]=c->reply[i];*written=c->reply_bytes;return 0;}}
 if(status==A04_USB_UNSUPPORTED){
 status=0;
 if(a04_usb_u32(p+12) || (p[6]==7 ? !n : n!=4))status=A04_USB_BAD_RANGE;
 else if(p[6]==7){if(!c->ops.push_rx || c->ops.push_rx(c->ops.context,p+32,n))status=A04_USB_BUSY;else count=n;}
 else {unsigned want=a04_usb_u32(p+32);
 if(!want || want>mps-48)status=A04_USB_BAD_RANGE;
 else if(!c->ops.pull_tx)status=A04_USB_BUSY;
 else {outn=c->ops.pull_tx(c->ops.context,o+48,want);if(outn>want)return A04_USB_BAD_FRAME;count=outn;}}
 if(!status)++s->next;
 }
 for(unsigned i=0;i<48;i++)o[i]=0;
 a04_usb_put(o,0x34305541);o[4]=1;o[6]=p[6];o[7]=0x80;
 a04_usb_put(o+8,a04_usb_u32(p+8));a04_usb_put(o+16,16+outn);
 a04_usb_put(o+32,status);a04_usb_put(o+36,s->next);a04_usb_put(o+40,count);
 a04_usb_put(o+20,~a04_usb_crc_update(~0U,o+32,16+outn));a04_usb_put(o+24,a04_usb_header_crc(o));
 *written=48+outn;
 if(!status){c->request_bytes=(unsigned)bytes;c->reply_bytes=*written;
 for(unsigned i=0;i<bytes;i++)c->request[i]=p[i];for(unsigned i=0;i<*written;i++)c->reply[i]=o[i];}
 return 0;
}
#endif
