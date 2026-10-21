/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original bounded USB console state. Caller serializes all operations.
 * No disk/bootwriter or arbitrary-memory operation exists here.
 */
#ifndef A04BR3_USB_STREAM_H
#define A04BR3_USB_STREAM_H
#include <stdint.h>
#include <stddef.h>
#define A04_USB_RING_SIZE 16384u
struct a04_usb_ring { uint8_t bytes[A04_USB_RING_SIZE]; unsigned head,used; };
struct a04_usb_stream {
 struct a04_usb_ring rx,tx;
 uint64_t generation,opened_generation;
 unsigned online,opened,failed,pending_in;
};
static inline void a04_usb_ring_clear(struct a04_usb_ring *r) {
 for(unsigned i=0;i<A04_USB_RING_SIZE;i++)r->bytes[i]=0;
 r->head=r->used=0;
}
static inline unsigned a04_usb_ring_put(struct a04_usb_ring *r,const void *data,unsigned count) {
 if(!data || count>A04_USB_RING_SIZE-r->used)return 0;
 const uint8_t *p=(const uint8_t *)data;
 for(unsigned i=0;i<count;i++)r->bytes[(r->head+r->used+i)%A04_USB_RING_SIZE]=p[i];
 r->used+=count;return count;
}
static inline unsigned a04_usb_ring_get(struct a04_usb_ring *r,void *data,unsigned cap) {
 unsigned n=r->used<cap?r->used:cap;if(!data)return 0;uint8_t *p=(uint8_t *)data;
 for(unsigned i=0;i<n;i++){p[i]=r->bytes[r->head];r->bytes[r->head]=0;r->head=(r->head+1)%A04_USB_RING_SIZE;}
 r->used-=n;return n;
}
static inline void a04_usb_stream_reset(struct a04_usb_stream *s) {
 a04_usb_ring_clear(&s->rx);a04_usb_ring_clear(&s->tx);
 s->pending_in=0;
 if(s->generation==UINT64_MAX){s->failed=1;s->online=0;}else ++s->generation;
}
static inline int a04_usb_stream_stale(const struct a04_usb_stream *s) {
 return !s->opened || s->opened_generation!=s->generation || s->failed;
}
#ifdef __cplusplus
extern "C" {
#endif
unsigned a04br3_usb_console_rx(const void *,unsigned);
unsigned a04br3_usb_console_tx(void *,unsigned);
void a04br3_usb_console_tx_complete(void);
unsigned a04br3_usb_console_rx_space(void);
void a04br3_usb_console_online(unsigned);
uint64_t a04br3_usb_console_generation(void);
void a04br3_usb_init(void);
#ifdef __cplusplus
}
#endif
#endif
