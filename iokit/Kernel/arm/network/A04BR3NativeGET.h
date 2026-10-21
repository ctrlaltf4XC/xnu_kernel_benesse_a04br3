/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * iOS16 IO80211 GetWithIOCTL codecs observed with REA at0x1d3ec3f64.
 * Type19: version/count/four state words,24bytes; type43:256byte C string.
 * Only observed state and this driver's own identity are returned. No invented
 * scan, RSSI, capability bits, Apple firmware version or association status.
 */
#ifndef A04BR3_NATIVE_GET_H
#define A04BR3_NATIVE_GET_H
#include "A04BR3NetworkBackend.h"
#include <sys/errno.h>
#include <string.h>
static inline void a04_native_put32(uint8_t *p,uint32_t n)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static inline int a04_native_get(const struct a04_network_state *s,
    uint32_t type,uint32_t value,void *out,uint32_t *bytes)
{
    if(!s || !out || !bytes)return EINVAL;
    if(!a04_network_control_ready(s))return ENETDOWN;
    if(value)return EOPNOTSUPP;
    unsigned size=type==0x13?24:(type==0x2b?256:0);
    if(!size)return EOPNOTSUPP;
    if(*bytes<size)return EMSGSIZE;
    uint8_t *p=(uint8_t *)out;
    if(type==0x13 && (p[0]!=1 || p[1] || p[2] || p[3]))return EINVAL;
    memset(p,0,size);
    if(type==0x13){a04_native_put32(p,1);a04_native_put32(p+4,1);a04_native_put32(p+8,!!s->radio_enabled);}
    else {static const char identity[]="A04BR3 MT6630 source SDIO runtime";memcpy(p,identity,sizeof(identity));}
    *bytes=size;return 0;
}
#endif
