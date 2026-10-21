/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original codec using retained iOS16 REA scan/helper/policy evidence and
 * corroborating itlwm53c51c2 apple80211_var.h (mac12+ exact1164) + producer.
 * No code copied from itlwm. Scalar offsets/constants are reproduced from the
 * observed ABI; reference hashes/credits are retained in worker logs.
 * Only board's observed ordinary beacon/probe 2.4GHz subset is supported.
 * Optional native rate words remain absent (count0) until conversion is
 * proven; actual rate IEs are preserved in full. No optional bandwidth,
 * active/DFS flags or noise/SNR measurement is invented. Full IEs are retained.
 */
#ifndef A04BR3_NATIVE_SCAN_RESULTS_H
#define A04BR3_NATIVE_SCAN_RESULTS_H
#include "A04BR3ScanObservation.h"
#include <sys/errno.h>
#include <string.h>
static inline uint32_t a04_scan_result_u32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void a04_scan_result_put16(uint8_t *p,uint16_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void a04_scan_result_put32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
/* 1=active,2=passive,3=cached, corroborated source enum + donor type3 wait
 * bypass. Only canonical unfiltered version1/BSSany/PHY1 request is accepted.
 * An unsupported policy leaves *mode unchanged, never silently drops filters.
 */
static inline int a04_scan_result_policy(uint32_t value,const void *in,
    uint32_t bytes,unsigned *mode)
{
    if(!in || !mode || bytes!=5456)return EINVAL;
    if(value)return EOPNOTSUPP;
    const uint8_t *p=(const uint8_t *)in;
    if(a04_scan_result_u32(p)!=1)return EINVAL;
    unsigned type=a04_scan_result_u32(p+0x34);
    if(type<1 || type>3)return EOPNOTSUPP;
    for(unsigned i=4;i<bytes;i++){
        uint8_t want=i==4?3:(i==0x34?(uint8_t)type:(i==0x38?1:0));
        if(p[i]!=want)return EOPNOTSUPP;
    }
    *mode=type;return 0;
}
static inline int a04_scan_result_encode(const struct a04_scan_snapshot *s,
    const struct a04_scan_observation *r,void *out,uint32_t *bytes)
{
    if(!s || !r || !out || !bytes || *bytes!=1164)return EINVAL;
    uint8_t *p=(uint8_t *)out;
    if(a04_scan_result_u32(p)!=1 || ((unsigned)p[0x8a]|((unsigned)p[0x8b]<<8))!=1024)return EINVAL;
    if(!s->generation || r->generation!=s->generation)return ESTALE;
    if(!s->complete)return EAGAIN;
    if(r->seen_us>s->now_us || r->ssid_length>32 || r->rate_count>15 ||
       r->ie_length<2 || r->ie_length>1024 || r->ie_truncated || r->from_probe>1 ||
       r->beacon>65535 || r->capability>65535)return EINVAL;
    if(r->band!=1 || !r->channel || r->channel>13 || !r->rcpi_valid || r->rcpi>220)return EOPNOTSUPP;
    unsigned any=0;for(unsigned i=0;i<6;i++)any|=r->bssid[i];
    if(!any || (r->bssid[0]&1))return EINVAL;
    // Full captured TLVs must agree with separately observed metadata. This
    // prevents accidentally combining an updated cache entry with older IEs.
    unsigned ssids=0,rates=0;
    for(unsigned i=0;i<r->ie_length;){
        if(r->ie_length-i<2 || (unsigned)r->ies[i+1]>r->ie_length-i-2)return EINVAL;
        unsigned id=r->ies[i],n=r->ies[i+1];const uint8_t *data=r->ies+i+2;
        if(id==0){
            if(ssids++ || n!=r->ssid_length || memcmp(data,r->ssid,n))return EINVAL;
        }
        if(id==1 || id==50){
            if(!n || rates+n>r->rate_count || memcmp(data,r->rates+rates,n))return EINVAL;
            rates+=n;
        }
        if(id==3 && (n!=1 || data[0]!=r->channel))return EINVAL;
        i+=2+n;
    }
    if(!ssids || !rates || rates!=r->rate_count)return EINVAL;
    uint64_t age=(s->now_us-r->seen_us)/1000;
    if(age>UINT32_MAX)age=UINT32_MAX;
    memset(p,0,1164);a04_scan_result_put32(p,1);a04_scan_result_put32(p+4,1);
    a04_scan_result_put32(p+8,r->channel);
    a04_scan_result_put32(p+0xc,8); // observed 2GHz band, no inferred width/active
    a04_scan_result_put16(p+0x16,(uint16_t)(int16_t)((int)(r->rcpi>>1)-110));
    a04_scan_result_put16(p+0x18,(uint16_t)r->beacon);
    a04_scan_result_put16(p+0x1a,(uint16_t)r->capability);
    memcpy(p+0x1c,r->bssid,6);
    // Native helper expects integer Mbps; reference rate-copy loop precedes
    // count assignment and cannot prove conversion. Optional count0 omits
    // those derived words; complete received rate IEs remain unchanged below.
    p[0x22]=0;
    p[0x60]=r->ssid_length;memcpy(p+0x61,r->ssid,r->ssid_length);
    a04_scan_result_put32(p+0x84,(uint32_t)age);
    a04_scan_result_put16(p+0x88,(uint16_t)r->from_probe); // donor probe-origin bit0
    a04_scan_result_put16(p+0x8a,r->ie_length);memcpy(p+0x8c,r->ies,r->ie_length);
    return 0;
}
struct a04_scan_result_session {
    uint64_t generation;
    uint32_t cursor,count,pinned;
};
static inline int a04_scan_result_accept(struct a04_scan_result_session *session,
    const struct a04_scan_snapshot *snapshot,unsigned cached)
{
    if(!session || !snapshot || !snapshot->generation)return EINVAL;
    if(cached && !snapshot->complete)return EAGAIN;
    *session={snapshot->generation,0,snapshot->complete?snapshot->count:0,snapshot->complete?1u:0u};
    return 0;
}
/* Get snapshot/record atomically from backend for session.generation and
 * session.cursor first. Do not fetch record when cursor already reaches count.
 * EIO terminates iteration; EAGAIN is pending; ESTALE must not restart at AP0.
 */
static inline int a04_scan_result_next(struct a04_scan_result_session *session,
    const struct a04_scan_snapshot *snapshot,const struct a04_scan_observation *r,
    void *out,uint32_t *bytes)
{
    if(!session || !snapshot || !session->generation)return EINVAL;
    if(session->generation!=snapshot->generation)return ESTALE;
    if(!snapshot->complete)return EAGAIN;
    if(session->pinned && snapshot->count!=session->count)return ESTALE;
    if(session->cursor>=snapshot->count)return EIO;
    int error=a04_scan_result_encode(snapshot,r,out,bytes);
    if(!error){session->count=snapshot->count;session->pinned=1;session->cursor++;}
    return error;
}
#endif
