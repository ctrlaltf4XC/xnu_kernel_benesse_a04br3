/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Offline-only, restricted native-format0x22 colour producer extension.
 * Original board libsrv_um.so FUN1306c8 evidence
 * ev_a5ef8d74135844c95dcd9b546504ad58255a362e2895f28c85dd76c16b55d375:
 * format22 table selects PBEtype0/flags0; channel metadata reaches case0,
 * flags&1==0, byte-pack c0|c1<<8|c2<<16|c3<<24 (NOT float conversion).
 * FUN129f88 copies this into P+18/P+20, payload+08/+10. Destination was
 * zeroed before the converter, so only the first32bit colour word is set.
 * All dimensions/heap validation/programs/other task words remain the fixed
 * source-derived profile. No MMIO/submission or live owner API added. The
 * current live owner intentionally re-derives ONLY zero colour and rejects
 * this nonzero image. Hardware colour-output semantics remain unverified.
 */
#ifndef A04BR3_ROGUE_CLEAR_COLOUR_H
#define A04BR3_ROGUE_CLEAR_COLOUR_H
#include "A04BR3RogueTransferFill.h"
static inline int a04_rgx_tq_clear_colour_build(a04_rgx_tq_clear *out,
    const a04_rgx_render_resources *r,const a04_rgx_bif *pds,const a04_rgx_bif *usc,
    uint32_t c0,uint32_t c1,uint32_t c2,uint32_t c3)
{
    if((c0|c1|c2|c3)>255)return 1;
    uint32_t packed=c0|(c1<<8)|(c2<<16)|(c3<<24);
    /* A future measured pattern must differ from the A5 target poison. */
    if(packed==0xa5a5a5a5U)return 1;
    int error=a04_rgx_tq_clear_build(out,r,pds,usc);
    if(error)return error;
    a04_rgx_tq_put32((unsigned char *)&out->payload+8,packed);
    return 0;
}
#endif
