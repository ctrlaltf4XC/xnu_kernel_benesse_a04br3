/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Restricted original-board producer reconstruction: zero colour, native
 * format0x22 (REA format table:32bpp, four8bit channels, clear permitted),
 * linear32x32/pitch32pixels, single RT/sample, source_count0, no blending.
 * Source: original libsrv_um.so FUN129f88 +135818 +12bfb4 +13e2e4;
 * evidence/offset ledger in RENDER-UPSTREAM.json. Not a modern KMD stream.
 * No caller-supplied task words, colour formats, shader code or addresses.
 */
#ifndef A04BR3_ROGUE_TRANSFER_FILL_H
#define A04BR3_ROGUE_TRANSFER_FILL_H
#include "A04BR3RogueTransferABI.h"
#include "A04BR3RogueHeapMap.h"
#include "A04BR3RoguePixelEvent.h"
#include "A04BR3RogueEOT.h"
enum {A04_RGX_CLEAR_WIDTH=32,A04_RGX_CLEAR_HEIGHT=32,
      A04_RGX_CLEAR_BYTES=32*32*4,A04_RGX_CLEAR_POISON=0xa5};
struct a04_rgx_tq_clear {
    a04_rgx_tq_payload208 payload;
    a04_rgx_pixel_event pixel;
    uint8_t usc[A04_RGX_EOT_SINGLE_RT_BYTES];
};
static inline void a04_rgx_tq_put32(void *p,uint32_t v)
{for(unsigned n=0;n<4;n++)((unsigned char *)p)[n]=(unsigned char)(v>>(8*n));}
static inline void a04_rgx_tq_put64(void *p,uint64_t v)
{for(unsigned n=0;n<8;n++)((unsigned char *)p)[n]=(unsigned char)(v>>(8*n));}
static inline int a04_rgx_tq_clear_build(a04_rgx_tq_clear *out,
    const a04_rgx_render_resources *r,const a04_rgx_bif *pds,const a04_rgx_bif *usc)
{
    if(!a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) || !a04_rgx_render_view_valid(r) || !pds || !usc ||
       pds->va!=0xa800000000ULL || usc->va!=0xba00000000ULL ||
       r->bif.map_bytes!=65536 || pds->map_bytes!=65536 || usc->map_bytes!=65536 ||
       !usc->read_only || pds->read_only)return 1;
    if(a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)) ||
       a04_rgx_fw_alias(out,sizeof(*out),pds,sizeof(*pds)) ||
       a04_rgx_fw_alias(out,sizeof(*out),usc,sizeof(*usc)))return 1;
    const a04_rgx_bif heaps[]={r->bif,*pds,*usc};
    if(a04_rgx_heap_map_validate(heaps,3))return 1;
    for(auto &h:heaps) {
        const a04_rgx_render_buffer buffers[]={h.pc,h.pd,h.pt,h.data};
        for(auto &b:buffers)if(a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes))return 1;
        uint64_t pa=0;
        for(unsigned n=0;n<h.map_bytes;n+=4096)
            if(a04_rgx_bif_translate(&h,h.va+n,&pa) || pa!=h.data.pa+n)return 1;
    }
    if(a04_rgx_fw_alias(out,sizeof(*out),r->bank.cpu,r->bank.bytes))return 1;
    a04_rgx_tq_clear generated={};auto bytes=(unsigned char *)&generated.payload;
    /* Native FUN13f4c0 entry0x22: PBE format0, flags0, swizzle0/1/2/3.
     * FUN13e2e4 gives one shared-register state and two FW CSR words.
     * Original general-format flags8 force bit19 in state upper word.
     * No source-format-specific postpatch for dstflags0/native0x22. */
    uint64_t address=r->bif.va;
    uint32_t state0=(uint32_t)(address>>2);
    uint32_t state1=(uint32_t)(address>>34)|(1U<<19);
    uint64_t swizzle=(1ULL<<23)|(2ULL<<26)|(3ULL<<29);
    uint64_t csr0=(2ULL<<56)|((uint64_t)((32U-1)&0x1fffeU)<<33)|swizzle;
    uint64_t csr1=((uint64_t)(32U-1)<<32)|(32U-1);
    /* FUN12bfb4/FUN135818: origin0, extent2x2 sixteen-pixel tiles;
     * one output region -> min2, nextPow2=2, ten slots -> pixel mask and
     * four tiles-in-flight. These are producer fields, not guessed MMIO. */
    uint32_t tiles=4,slots=10;
    uint32_t pixelControl=(~((8U<<slots)-1U))^0xfffffffcU;
    a04_rgx_tq_put32(bytes+0x04,pixelControl);
    a04_rgx_tq_put32(bytes+0x18,2U|(2U<<16));
    a04_rgx_tq_put32(bytes+0x20,0xa0000U|((tiles-1U)<<12));
    /* Native single-RT PDS reservation: data5words padded32, code5words.
     * FUN129f88: ((1*64+480)&0x7e00) | ceil(dataWords/4). */
    uint32_t info=((1U*64U+480U)&0x7e00U)|((A04_RGX_PIXEL_EVENT_DATA_WORDS+3U)/4U);
    a04_rgx_tq_put32(bytes+0x28,info);
    a04_rgx_tq_put32(bytes+0x2c,A04_RGX_PIXEL_EVENT_CODE_OFFSET);
    a04_rgx_tq_put32(bytes+0x30,0); /* actual data at owned PDS base */
    a04_rgx_tq_put64(bytes+0x60,3); /* original zero-source P+0x70 */
    a04_rgx_tq_put64(bytes+0x80,csr0);
    a04_rgx_tq_put64(bytes+0x88,csr1);
    /* All remaining fields originate from zeroed native queue scratch:
     * no fragment/background tasks, zero colour, unused RTs, frame0, flags0. */
    if(a04_rgx_eot_single_rt_generate(generated.usc,sizeof(generated.usc)) ||
       a04_rgx_pixel_event_build(&generated.pixel,0,A04_RGX_EOT_SINGLE_RT_TEMPS,state0,state1))return 1;
    *out=generated;return 0;
}
/* Trusted owner only; payload must have passed the fixed producer validation.
 * Exact DDK TASK then UPDATE; no NULL on TQ_3D. Host write stays private and
 * only KCCB.CWoff publishes it to the firmware-owned CCCB_CTL.write. */
static inline int a04_rgx_tq_clear_enqueue(a04_rgx_render_resources *r,
    a04_rgx_render_ring_state *state,const a04_rgx_tq_clear *clear,RGXFWIF_KCCB_CMD *kick)
{
    if(!r || !state || !clear || !kick || !a04_rgx_render_view_valid(r) ||
       state->poisoned || !state->sequence ||
       (a04_rgx_fw_u32((const unsigned char *)&clear->payload+0xcc)&~0x39U))return 1;
    const a04_rgx_render_buffer all[]={r->bank,r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data};
    for(auto &b:all)if(a04_rgx_fw_alias(clear,sizeof(*clear),b.cpu,b.bytes) ||
        a04_rgx_fw_alias(state,sizeof(*state),b.cpu,b.bytes) ||
        a04_rgx_fw_alias(kick,sizeof(*kick),b.cpu,b.bytes))return 1;
    if(a04_rgx_fw_alias(clear,sizeof(*clear),state,sizeof(*state)) ||
       a04_rgx_fw_alias(clear,sizeof(*clear),kick,sizeof(*kick)) ||
       a04_rgx_fw_alias(kick,sizeof(*kick),state,sizeof(*state)))return 1;
    auto ctl=(volatile RGXFWIF_CCCB_CTL *)r->control[3];unsigned at=state->write[3];
    if(ctl->ui32WrapMask!=4095 || at>4095 || (at&7) || ctl->ui32ReadOffset!=at ||
       ctl->ui32WriteOffset!=at || ctl->ui32DepOffset>4095 || (ctl->ui32DepOffset&7) ||
       4096-at<40)return 2;
    if(at+296+40>4096) {
        RGXFWIF_CCB_CMD_HEADER pad={};pad.eCmdType=A04_RGX_CLIENT_PADDING;pad.ui32CmdSize=4096-at-40;
        a04_rgx_fw_zero(r->commands[3]+at,4096-at);
        *(RGXFWIF_CCB_CMD_HEADER *)(r->commands[3]+at)=pad;at=0;
    }
    if(a04_rgx_tq_encode_packet(&clear->payload,208,r->commands[3]+at,248))return 1;
    RGXFWIF_CCB_CMD_HEADER update={};update.eCmdType=A04_RGX_CLIENT_UPDATE;update.ui32CmdSize=8;
    *(RGXFWIF_CCB_CMD_HEADER *)(r->commands[3]+at+248)=update;
    auto ufo=(RGXFWIF_UFO *)(r->commands[3]+at+288);ufo->puiAddrUFO=r->sync_addr;
    ufo->ui32Value=state->sequence;*r->sync=0;
    RGXFWIF_KCCB_CMD cmd={};cmd.eCmdType=RGXFWIF_KCCB_CMD_KICK;cmd.eDM=3;
    cmd.uCmdData.sCmdKickData.psContext.ui32Addr=a04_rgx_render_fw_address(r,r->tq);
    cmd.uCmdData.sCmdKickData.ui32CWoffUpdate=at+296;*kick=cmd;state->write[3]=at+296;
    return 0;
}
#endif
