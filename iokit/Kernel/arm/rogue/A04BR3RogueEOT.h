/* SPDX-License-Identifier: MIT
 * Full permission notice: LICENSE.Mesa in this directory.
 * Copyright (c) 2024 Imagination Technologies Ltd.
 * Copyright (c) 2026 A04BR3 contributors.
 * Forward assembly of the single-RT emitpix EOT, from MIT Mesa PCO
 * e1ef8f046c6e37ec1e2a13229560fbb824e5118f pco_isa.py, pco_map.py,
 * pco_end.c and pco_binary.c. Named-field source assembly exactly matches
 * old board libsrv_um count1 program at15060c, consumed through context54b8.
 * No extracted asset or USC disassembly is used by this generator.
 * Only shared PBE words0/1, freep, instance execution, zero temps are proven.
 * No launch/clear/completion implied. Consumer owns USC heap mapping/cache,
 * exact PBE state and task submission. Not a general USC assembler.
 */
#ifndef A04BR3_ROGUE_EOT_H
#define A04BR3_ROGUE_EOT_H
#include <stdint.h>
enum { A04_RGX_EOT_SINGLE_RT_BYTES=12,A04_RGX_EOT_SINGLE_RT_TEMPS=0 };
static inline int a04_rgx_eot_single_rt_generate(uint8_t *out,unsigned capacity)
{
    if(!out || capacity<A04_RGX_EOT_SINGLE_RT_BYTES)return 1;
    const unsigned shared_bank=4,special_bank=0;
    const unsigned state0=0,state1=1,backend_emit=6,oporg_backend=2;
    /* Main header, DA=hdr3+backend1, length=(3+1+5+1+1+padding1)/2.
     * ext=1, OLCHK skipped by EOT source, end=1, rpt1 encodes0. */
    out[0]=(uint8_t)((4u<<4)|6u);
    out[1]=(uint8_t)((1u<<7)|(oporg_backend<<4));
    out[2]=(uint8_t)(1u<<7);
    /* I_EMITPIX: backend_op emit, reserved0, freep1. */
    out[3]=(uint8_t)((backend_emit<<5)|(1u<<1));
    /* I_THREE_LO_3B8I_2B8I_3B8I_3M from source-first variant selection.
     * s0=shared0, s2=shared1; unused s1=special0 and is0=0.
     * ext0/ext1/ext2=1, sel/ext4=0. Map the named bank/index pieces. */
    out[4]=(uint8_t)((1u<<7)|((shared_bank&1)<<6)|(state0&63));
    out[5]=(uint8_t)((1u<<6)|((special_bank&1)<<5));
    out[6]=(uint8_t)((1u<<7)|(((shared_bank>>1)&1)<<4)|
                    (((special_bank>>1)&1)<<3)|(((state0>>6)&1)<<2));
    out[7]=(uint8_t)(((shared_bank&3)<<6)|(state1&63));
    out[8]=(uint8_t)((((shared_bank>>2)&1)<<7)|(((state1>>6)&3)<<5)|
                    (((shared_bank>>2)&1)<<3)|(((state0>>7)&1)<<1));
    /* I_ONE_UP_1B6I absent upper references; ISS selectors absent =>0. */
    out[9]=0;out[10]=0;
    /* pco_encode_align: one odd-byte word pad, not an opcode or asset. */
    out[11]=0xff;return 0;
}
#endif
