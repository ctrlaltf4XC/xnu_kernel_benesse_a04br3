/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Cold, shared-catalogue mappings for independent Rogue address-space heaps.
 * Uses the matching DDK 4K BIF encoding already defined by RogueBIF.h.
 * Caller owns fresh unpublished tables and must clean/publish them before FW
 * start. No live TLB changes, MMIO, inferred heap bases or executable bytes.
 */
#ifndef A04BR3_ROGUE_HEAP_MAP_H
#define A04BR3_ROGUE_HEAP_MAP_H
#include "A04BR3RogueBIF.h"
enum { A04_RGX_MAX_COLD_HEAPS=4 };
static inline int a04_rgx_heap_map_validate(const a04_rgx_bif *regions,unsigned count)
{
    if (!regions || !count || count>A04_RGX_MAX_COLD_HEAPS) return 1;
    for (unsigned i=0;i<count;i++) {
        const auto &r=regions[i];
        if (a04_rgx_bif_validate(&r) || r.pc.cpu!=regions[0].pc.cpu ||
            r.pc.pa!=regions[0].pc.pa || r.pc.bytes!=regions[0].pc.bytes) return 1;
        const a04_rgx_render_buffer buffers[]={r.pc,r.pd,r.pt,r.data};
        for (unsigned b=0;b<4;b++)
            if (a04_rgx_fw_alias(regions,count*sizeof(*regions),buffers[b].cpu,buffers[b].bytes)) return 1;
        for (unsigned j=0;j<i;j++) {
            const auto &old=regions[j];
            /* Distinct PCEs give each heap its own PD; sharing one is not
             * implemented here. Each region itself stays inside one PCE. */
            if ((r.va>>30)==(old.va>>30)) return 1;
            const a04_rgx_render_buffer previous[]={old.pc,old.pd,old.pt,old.data};
            for (unsigned a=0;a<4;a++) for (unsigned b=0;b<4;b++) {
                if (!a && !b) continue; /* exact shared catalogue only */
                if (a04_rgx_render_buffers_overlap(buffers[a],previous[b])) return 1;
            }
        }
    }
    return 0;
}
static inline int a04_rgx_heap_map_build(const a04_rgx_bif *regions,unsigned count)
{
    if (a04_rgx_heap_map_validate(regions,count)) return 1;
    /* Complete validation before any write: a rejected set preserves all
     * candidate tables and data, including an already constructed baseline. */
    a04_rgx_fw_zero(regions[0].pc.cpu,regions[0].pc.bytes);
    for (unsigned i=0;i<count;i++) {
        const auto &r=regions[i];
        a04_rgx_fw_zero(r.pd.cpu,r.pd.bytes);
        a04_rgx_fw_zero(r.pt.cpu,r.pt.bytes);
        auto pc=(uint32_t *)r.pc.cpu;auto pd=(uint64_t *)r.pd.cpu;
        auto pt=(uint64_t *)r.pt.cpu;
        pc[r.va>>30]=(uint32_t)(r.pd.pa>>8)|1U;
        const unsigned first=(unsigned)((r.va>>21)&511);
        const unsigned tables=(unsigned)(((r.va&0x1fffff)+r.map_bytes+0x1fffff)>>21);
        for (unsigned n=0;n<tables;n++) pd[first+n]=(r.pt.pa+n*4096)|1ULL;
        const unsigned start=(unsigned)((r.va>>12)&511);
        for (unsigned n=0;n<r.map_bytes/4096;n++)
            pt[start+n]=(r.data.pa+n*4096)|9ULL|(r.read_only?2ULL:0ULL);
    }
    return 0;
}
#endif
