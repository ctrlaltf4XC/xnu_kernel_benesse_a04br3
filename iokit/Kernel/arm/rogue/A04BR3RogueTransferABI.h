/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Original board libsrv_um.so (SHA256 d32a4a224d06302d9bec0a66cbb7d17dec663d3985f1c8592f5bb77353ca1d4b)
 * RGXSubmitTransfer0x1326a8, REA ev_81c5443c85f9a0d64be59e6b70c814460dac832358891dd322fedb68604cda18.
 * This is an OFFLINE producer/layout encoder, NOT validation of register words
 * for hardware. No live submission entry accepts this opaque payload. Register
 * identities/EOT/PDS producer semantics must be established before that step.
 * DDK rgxtransfer.c supplies opaque FW bytes to rgxccb.c with TQ_3D type.
 */
#ifndef A04BR3_ROGUE_TRANSFER_ABI_H
#define A04BR3_ROGUE_TRANSFER_ABI_H
#include "A04BR3RogueRenderResources.h"
struct a04_rgx_tq_payload208 {
    uint64_t register_words[25]; /* original200byte copy; no invented field names */
    uint32_t producer_word_c8;   /* source record+0x14; meaning not established */
    uint32_t flags;             /* bit0 from record+0x10 bit3; encoded hint in3..5 */
};
static_assert(sizeof(a04_rgx_tq_payload208)==208,"original transfer payload size");
static_assert(offsetof(a04_rgx_tq_payload208,producer_word_c8)==0xc8,"original tail");
static_assert(offsetof(a04_rgx_tq_payload208,flags)==0xcc,"original flags, not0xc4");
enum {A04_RGX_TQ3D_TASK=0x2abc80cc,A04_RGX_TQ3D_PACKET_BYTES=248};
static inline int a04_rgx_tq_encode_record(const void *record,size_t recordBytes,
    uint32_t encodedHint,void *payload,size_t payloadBytes)
{
    /* Complete bounds/type preflight BEFORE any record load or output write. */
    if(recordBytes<0xe8 || payloadBytes!=208 || encodedHint>7 ||
       !a04_rgx_fw_pointer(record,recordBytes) || !a04_rgx_fw_pointer(payload,payloadBytes) ||
       a04_rgx_fw_alias(record,recordBytes,payload,payloadBytes))return 1;
    auto in=(const unsigned char *)record;auto out=(unsigned char *)payload;
    if(a04_rgx_fw_u32(in+0x1c)!=2)return 1;
    for(unsigned n=0;n<200;n++)out[n]=in[0x20+n];
    for(unsigned n=0;n<4;n++)out[0xc8+n]=in[0x14+n];
    uint32_t flags=((a04_rgx_fw_u32(in+0x10)>>3)&1)|(encodedHint<<3);
    for(unsigned n=0;n<4;n++)out[0xcc+n]=(unsigned char)(flags>>(8*n));
    return 0;
}
static inline int a04_rgx_tq_encode_packet(const void *payload,size_t payloadBytes,
    void *packet,size_t packetBytes)
{
    if(payloadBytes!=208 || packetBytes!=248 || !a04_rgx_fw_pointer(payload,payloadBytes) ||
       !a04_rgx_fw_pointer(packet,packetBytes) || ((uintptr_t)packet&7) ||
       a04_rgx_fw_alias(payload,payloadBytes,packet,packetBytes))return 1;
    /* Reject tail encodings the original producer cannot emit. This still
     * says NOTHING about validity of the opaque200 register bytes. */
    if(a04_rgx_fw_u32((const unsigned char *)payload+0xcc)&~0x39U)return 1;
    RGXFWIF_CCB_CMD_HEADER header={};header.eCmdType=A04_RGX_TQ3D_TASK;header.ui32CmdSize=208;
    *(RGXFWIF_CCB_CMD_HEADER *)packet=header;
    for(unsigned n=0;n<208;n++)((unsigned char *)packet)[40+n]=((const unsigned char *)payload)[n];
    return 0;
}
#endif
