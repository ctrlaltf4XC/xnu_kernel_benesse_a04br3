/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native Rogue 40-bit VA / 32-bit PA BIF 4KiB tables, not MIPS tables.
 * Source: Imagination MIT-option m1.11ED5425693 rgxmmuinit.c 4KBDP
 * PCE/PDE/PTE configuration and rgxmmudefs_km.h. No MMIO or live remapping.
 */
#ifndef A04BR3_ROGUE_BIF_H
#define A04BR3_ROGUE_BIF_H
#include "A04BR3RogueFirmware.h"
#include "A04BR3RogueMipsMMU.h"
struct a04_rgx_render_buffer {void *cpu;uint64_t pa;uint32_t bytes;};
struct a04_rgx_bif {
    a04_rgx_render_buffer pc,pd,pt,data;
    uint64_t va;
    uint32_t map_bytes;
    bool read_only;
};
static inline bool a04_rgx_render_buffer_valid(const a04_rgx_render_buffer &b)
{
    return b.bytes && !(b.bytes&4095) && !((uintptr_t)b.cpu&4095) &&
        a04_rgx_fw_pointer(b.cpu,b.bytes) &&
        a04_rgx_mips_physical_range(b.pa,b.bytes,4096) &&
        !a04_rgx_fw_overlap(b.pa,b.bytes,0x1fc00000,0x4000);
}
static inline bool a04_rgx_render_buffers_overlap(const a04_rgx_render_buffer &a,
                                                 const a04_rgx_render_buffer &b)
{
    return a04_rgx_fw_overlap(a.pa,a.bytes,b.pa,b.bytes) ||
        a04_rgx_fw_alias(a.cpu,a.bytes,b.cpu,b.bytes);
}
static inline int a04_rgx_bif_validate(const a04_rgx_bif *b)
{
    if (!b || !b->map_bytes || (b->map_bytes&4095) || (b->va&4095) ||
        b->va>=0x10000000000ULL || b->map_bytes>0x2000000U ||
        b->map_bytes>0x10000000000ULL-b->va || b->data.bytes<b->map_bytes ||
        b->pc.bytes!=4096 || b->pd.bytes!=4096 || b->pt.bytes>16*4096U ||
        (b->va>>30)!=((b->va+b->map_bytes-1)>>30)) return 1;
    const a04_rgx_render_buffer list[]={b->pc,b->pd,b->pt,b->data};
    for (unsigned i=0;i<4;i++) {
        if (!a04_rgx_render_buffer_valid(list[i])) return 1;
        for (unsigned j=0;j<i;j++) if (a04_rgx_render_buffers_overlap(list[i],list[j])) return 1;
    }
    uint32_t tables=(uint32_t)(((b->va&0x1fffff)+b->map_bytes+0x1fffff)>>21);
    return b->pt.bytes<tables*4096U;
}
static inline int a04_rgx_bif_build(const a04_rgx_bif *input)
{
    if (!input) return 1;
    a04_rgx_bif local=*input;const a04_rgx_bif *b=&local;
    if (a04_rgx_bif_validate(b)) return 1;
    /* Atomic preflight: caller supplies fresh, unpublished tables. */
    a04_rgx_fw_zero(b->pc.cpu,b->pc.bytes);
    a04_rgx_fw_zero(b->pd.cpu,b->pd.bytes);
    a04_rgx_fw_zero(b->pt.cpu,b->pt.bytes);
    auto pc=(uint32_t *)b->pc.cpu;auto pd=(uint64_t *)b->pd.cpu;
    auto pt=(uint64_t *)b->pt.cpu;
    pc[b->va>>30]=(uint32_t)(b->pd.pa>>8)|1;
    unsigned first=(unsigned)((b->va>>21)&511);
    unsigned tables=(unsigned)(((b->va&0x1fffff)+b->map_bytes+0x1fffff)>>21);
    for (unsigned n=0;n<tables;n++) pd[first+n]=(b->pt.pa+n*4096)|1ULL;
    unsigned start=(unsigned)((b->va>>12)&511);
    for (unsigned n=0;n<b->map_bytes/4096;n++)
        pt[start+n]=(b->data.pa+n*4096)|9ULL|(b->read_only?2ULL:0ULL);
    /* Valid + SLC bypass, no coherent/snoop/protected flags, optional RO.
     * PC address encoding differs from PDE/PTE: PA>>12 then <<4. */
    return 0;
}
/* Host verifier/walker used before a future GPU task consumes this address space.
 * Does not dereference addresses obtained from table contents. */
static inline int a04_rgx_bif_translate(const a04_rgx_bif *b,uint64_t va,uint64_t *pa)
{
    if (!pa || a04_rgx_bif_validate(b) || va<b->va || va-b->va>=b->map_bytes) return 1;
    auto pc=(const uint32_t *)b->pc.cpu;auto pd=(const uint64_t *)b->pd.cpu;
    auto pt=(const uint64_t *)b->pt.cpu;
    unsigned n=(unsigned)((va>>21)-(b->va>>21));
    if (pc[va>>30]!=((uint32_t)(b->pd.pa>>8)|1) ||
        pd[(va>>21)&511]!=((b->pt.pa+n*4096)|1ULL)) return 1;
    unsigned at=(unsigned)((va-(b->va&~0x1fffffULL))>>12);
    uint64_t expected=(b->data.pa+((va-b->va)&~4095ULL))|9ULL|(b->read_only?2ULL:0ULL);
    if (pt[at]!=expected) return 1;
    *pa=(expected&0xfffffff000ULL)|(va&4095);return 0;
}
#endif
