/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026 A04BR3 contributors.
 * Original board m1.11 libsrv_um.so FUN_00129f88, not modern KMD stream ABI.
 * See A04BR3RogueFragmentKick.sources.json for source/evidence/limits.
 * Pure CPU packing of four old208 background fields and the shader-branch
 * raw00 value. Does NOT produce a complete valid task or authorize submission.
 */
#ifndef A04BR3_ROGUE_TRANSFER_BACKGROUND_H
#define A04BR3_ROGUE_TRANSFER_BACKGROUND_H
#include <stdint.h>
#include <stddef.h>
/* All addresses are offsets in ONE validated PDS heap. Caller must separately
 * prove ownership/mapping/cache state and contents. Extents bound arithmetic;
 * they do not certify instructions or describe general firmware resources.
 * first/second refer to original P+168/P+170; modern Mesa uniform/texture
 * nomenclature is deliberately not imposed on the target's older layout. */
struct a04_rgx_tq_background_input {
    uint64_t heap_bytes;
    uint64_t task_offset, task_bytes;       /* P+160: fragment data+code */
    uint64_t secondary_code, secondary_code_bytes; /* P+178 */
    uint64_t first_data, first_data_bytes;  /* P+168 */
    uint64_t second_data, second_data_bytes;/* P+170 */
    uint32_t first_words, second_words;    /* P+180/P+184, already padded */
    uint32_t shared_words, pds_temps;       /* P+18c/P+1a4 */
};
struct a04_rgx_tq_background_fields {
    uint64_t raw38, raw40, raw48, raw50;
    uint32_t shader_branch_raw00; /* native shader path writes raw00=0x200 */
    uint32_t reserved; /* canonical CPU result, NOT another raw208 field */
};
static inline int a04_rgx_tq_background_range(uint64_t offset,uint64_t bytes,
    uint64_t heap_bytes)
{
    return !bytes || (offset&15) || offset>=heap_bytes || bytes>heap_bytes-offset;
}
/* Narrow nonempty secondary-state path. No zero-address sentinel ambiguity:
 * offset0 is valid when backed by an explicit nonempty extent. No masks may
 * discard bits. Local conservative shared-store limit=256 DWORDs, not a claim
 * that this helper establishes BVNC resource capacity.
 * Exact output fields correspond to raw208 offsets, NOT named modern regs.
 * Inputs are snapshotted after full bounds checks; overlap and failure leave
 * all output bytes unchanged. All raw208/PBE/ISP/clip/task-tail bytes outside
 * these fields remain UNSPECIFIED by this API. No function submits its result.
 */
static inline int a04_rgx_tq_background_encode(
    struct a04_rgx_tq_background_fields *out,size_t out_bytes,
    const struct a04_rgx_tq_background_input *input,size_t input_bytes)
{
    uintptr_t o=(uintptr_t)out,i=(uintptr_t)input;
    if(!out || !input || out_bytes<sizeof(*out) || input_bytes<sizeof(*input) ||
       o>UINTPTR_MAX-sizeof(*out) || i>UINTPTR_MAX-sizeof(*input) ||
       (o%sizeof(uint64_t)) || (i%sizeof(uint64_t)) ||
       (o<i+sizeof(*input) && i<o+sizeof(*out)))return 1;
    struct a04_rgx_tq_background_input in=*input;
    if(!in.heap_bytes || in.heap_bytes>UINT64_C(0x100000000) ||
       in.task_bytes<36 || (in.task_bytes&3) || (in.secondary_code_bytes&3) ||
       a04_rgx_tq_background_range(in.task_offset,in.task_bytes,in.heap_bytes) ||
       a04_rgx_tq_background_range(in.secondary_code,in.secondary_code_bytes,in.heap_bytes) ||
       a04_rgx_tq_background_range(in.first_data,in.first_data_bytes,in.heap_bytes) ||
       a04_rgx_tq_background_range(in.second_data,in.second_data_bytes,in.heap_bytes) ||
       !in.first_words || (in.first_words&3) || in.first_words>2044 ||
       !in.second_words || (in.second_words&3) || in.second_words>508 ||
       in.first_data_bytes<(uint64_t)in.first_words*4 ||
       in.second_data_bytes<(uint64_t)in.second_words*4 ||
       !in.shared_words || in.shared_words>256 || !in.pds_temps || in.pds_temps>60)return 1;
    struct a04_rgx_tq_background_fields made={0,0,0,0,0,0};
    made.raw38=in.task_offset | (in.secondary_code<<32);
    made.raw40=in.second_data<<32;
    made.raw48=in.first_data;
    made.raw50=((uint64_t)in.first_words<<21) |
               ((uint64_t)((in.shared_words+15)/16)<<55) |
               ((uint64_t)((in.pds_temps+3)/4)) |
               ((uint64_t)(in.second_words/4)<<16);
    made.shader_branch_raw00=0x200;
    *out=made;return 0;
}
#endif
