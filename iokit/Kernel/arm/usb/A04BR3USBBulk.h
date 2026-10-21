/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original private CRC transport v1. NOT the boot updater ABI.
 * No disk, file, flash, physical-memory, commit or reboot callback exists.
 * RAM-free stream validation only; this cannot validate/install a boot image.
 * Caller owns a zeroed/reset session and serializes every call. All external
 * lengths checked before loads. One request per <=512-byte USB packet.
 */
#ifndef A04BR3_USB_BULK_H
#define A04BR3_USB_BULK_H
#include <stdint.h>
#include <stddef.h>
enum {A04_USB_PACKET=512,A04_USB_HEADER=32,A04_USB_REPLY=64,
 A04_USB_MAX_STREAM=32*1024*1024,
 A04_USB_PING=1,A04_USB_INFO=2,A04_USB_BEGIN=3,A04_USB_DATA=4,A04_USB_END=5,A04_USB_ABORT=6,
 A04_USB_OK=0,A04_USB_BAD_FRAME=1,A04_USB_BAD_SEQUENCE=2,A04_USB_BAD_STATE=3,
 A04_USB_BAD_RANGE=4,A04_USB_BAD_CRC=5,A04_USB_UNSUPPORTED=6};
struct a04_usb_session {uint32_t next,total,received,crc,expected,active;};
static inline uint32_t a04_usb_u32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void a04_usb_put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static inline uint32_t a04_usb_crc_update(uint32_t crc,const uint8_t *p,unsigned bytes){
 for(unsigned i=0;i<bytes;i++){crc^=p[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));}return crc;
}
static inline uint32_t a04_usb_header_crc(const uint8_t *p){
 uint32_t c=~0U;for(unsigned i=0;i<32;i++){uint8_t b=(i>=24&&i<28)?0:p[i];c=a04_usb_crc_update(c,&b,1);}return ~c;
}
static inline void a04_usb_session_reset(struct a04_usb_session *s){
 s->next=1;s->total=s->received=s->expected=s->active=0;s->crc=~0U;
}
static inline int a04_usb_span(const void *p,size_t n){return p && n && (uintptr_t)p<=UINTPTR_MAX-n;}
static inline int a04_usb_overlap(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a<(uintptr_t)b+m && (uintptr_t)b<(uintptr_t)a+n;}
/* Return BAD_FRAME means no output/state mutation. A valid request always
 * returns0 with a CRC-framed reply whose status is explicit. Error requests
 * do not advance sequence. END CRC success is TRANSPORT ONLY, not image proof. */
static inline int a04_usb_bulk_process(struct a04_usb_session *s,
 const void *request,size_t bytes,void *reply,size_t capacity,unsigned maxpacket){
 if((maxpacket!=64&&maxpacket!=512) || bytes<32 || bytes>maxpacket || capacity<64 ||
  !a04_usb_span(s,sizeof(*s)) || ((uintptr_t)s&3U) || !a04_usb_span(request,bytes) || !a04_usb_span(reply,64) ||
  a04_usb_overlap(s,sizeof(*s),request,bytes) || a04_usb_overlap(s,sizeof(*s),reply,64) ||
  a04_usb_overlap(request,bytes,reply,64))return A04_USB_BAD_FRAME;
 const uint8_t *p=(const uint8_t *)request;
 if(a04_usb_u32(p)!=0x34305541U || p[4]!=1 || p[5] || p[7] || a04_usb_u32(p+28) ||
  a04_usb_u32(p+16)!=bytes-32 || a04_usb_u32(p+24)!=a04_usb_header_crc(p) ||
  a04_usb_u32(p+20)!=~a04_usb_crc_update(~0U,p+32,(unsigned)bytes-32))return A04_USB_BAD_FRAME;
 uint32_t seq=a04_usb_u32(p+8),offset=a04_usb_u32(p+12),n=(uint32_t)bytes-32,status=0;
 struct a04_usb_session next=*s;
 if(!s->next || s->next==UINT32_MAX || seq!=s->next)status=A04_USB_BAD_SEQUENCE;
 else switch(p[6]){
 case A04_USB_PING:case A04_USB_INFO:
  if(n || offset)status=A04_USB_BAD_RANGE;break;
 case A04_USB_BEGIN:
  if(n!=8 || offset)status=A04_USB_BAD_RANGE;
  else if(s->active)status=A04_USB_BAD_STATE;
  else if(!a04_usb_u32(p+32) || a04_usb_u32(p+32)>A04_USB_MAX_STREAM)status=A04_USB_BAD_RANGE;
  else {next.total=a04_usb_u32(p+32);next.expected=a04_usb_u32(p+36);next.received=0;next.crc=~0U;next.active=1;}break;
 case A04_USB_DATA:
  if(!s->active)status=A04_USB_BAD_STATE;
  else if(!n || s->received>s->total || offset!=s->received || n>s->total-s->received)status=A04_USB_BAD_RANGE;
  else {next.crc=a04_usb_crc_update(s->crc,p+32,n);next.received+=n;}break;
 case A04_USB_END:
  if(n || !s->active || offset!=s->total || s->received!=s->total)status=A04_USB_BAD_STATE;
  else if((~s->crc)!=s->expected)status=A04_USB_BAD_CRC;
  else next.active=0;break;
 case A04_USB_ABORT:
  if(n || offset)status=A04_USB_BAD_RANGE;
  else {a04_usb_session_reset(&next);next.next=s->next;}break;
 default:status=A04_USB_UNSUPPORTED;break;
 }
 if(!status){++next.next;*s=next;}
 uint8_t *out=(uint8_t *)reply;for(unsigned i=0;i<64;i++)out[i]=0;
 a04_usb_put(out,0x34305541U);out[4]=1;out[6]=p[6];out[7]=0x80;
 a04_usb_put(out+8,seq);a04_usb_put(out+12,s->received);a04_usb_put(out+16,32);
 a04_usb_put(out+32,status);a04_usb_put(out+36,s->next);a04_usb_put(out+40,s->total);
 a04_usb_put(out+44,s->received);a04_usb_put(out+48,~s->crc);a04_usb_put(out+52,s->active);
 a04_usb_put(out+56,A04_USB_MAX_STREAM);a04_usb_put(out+60,maxpacket-32);
 a04_usb_put(out+20,~a04_usb_crc_update(~0U,out+32,32));a04_usb_put(out+24,a04_usb_header_crc(out));return 0;
}
#endif
