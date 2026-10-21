/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native TA/3D resources and a typed client-CCB UPDATE/KCCB KICK constructor.
 * Sources: MIT-option m1.11ED5425693 rgxmem.c (FW memory context),
 * rgxfwutils.c FWCommonContextAllocate, rgxta3d.c _CreateTA/3DContext and
 * RGXKickTA3DKM, rgxccb.c padding/reserved-header rule, rgx_fwif_km.h UFOs.
 * No shader/TA/3D task bytes accepted. UPDATE exercises the render-context
 * firmware scheduler, never claims a draw. GPU task ABI is a separate step.
 */
#ifndef A04BR3_ROGUE_RENDER_RESOURCES_H
#define A04BR3_ROGUE_RENDER_RESOURCES_H
#include "A04BR3RogueBIF.h"
#include "A04BR3RogueRenderABI.h"
#include "A04BR3RogueRuntimeLayout.h"
static constexpr uint64_t A04_RGX_RENDER_SCRATCH_VA=0x65c0000000ULL; /* rgxheapconfig.h BRN65273 */
enum {A04_RGX_RENDER_BANK_BYTES=65536,A04_RGX_RENDER_FW_OFFSET=0x80000,
      A04_RGX_CLIENT_BYTES=4096,A04_RGX_CLIENT_NULL=0x2abc80cf,
      A04_RGX_CLIENT_UPDATE=0x2abc00d6,A04_RGX_CLIENT_PRIORITY=0x2abc00d9,
      A04_RGX_CLIENT_PADDING=0x2abc00dd};
struct a04_rgx_render_resources {
    a04_rgx_render_buffer bank;
    a04_rgx_bif bif;
    RGXFWIF_FWRENDERCONTEXT *context;
    RGXFWIF_FWCOMMONCONTEXT *gp; /* exact rgxkicksync.c DM_GP context */
    RGXFWIF_FWMEMCONTEXT *memory;
    RGXFWIF_CCCB_CTL *control[3];
    unsigned char *commands[3];
    volatile uint32_t *sync;
    RGXFWIF_DEV_VIRTADDR context_addr,memory_addr,sync_addr;
    uint32_t used;
};
static inline uint32_t a04_rgx_render_fw_address(const a04_rgx_render_resources *r,const void *p)
{
    return 0xc0000000U+A04_RGX_RENDER_FW_OFFSET+(uint32_t)((uintptr_t)p-(uintptr_t)r->bank.cpu);
}
static inline int a04_rgx_render_validate(const a04_rgx_render_buffer *bank,const a04_rgx_bif *b)
{
    if (!bank || bank->bytes!=A04_RGX_RENDER_BANK_BYTES || !a04_rgx_render_buffer_valid(*bank) ||
        a04_rgx_bif_validate(b) || b->read_only ||
        b->va<A04_RGX_RENDER_SCRATCH_VA || b->va+b->map_bytes>A04_RGX_RENDER_SCRATCH_VA+0x80000000ULL) return 1;
    const a04_rgx_render_buffer list[]={b->pc,b->pd,b->pt,b->data};
    for (unsigned i=0;i<4;i++) if (a04_rgx_render_buffers_overlap(*bank,list[i])) return 1;
    return 0;
}
static inline int a04_rgx_render_build(a04_rgx_render_resources *out,
    const a04_rgx_render_buffer *bank,const a04_rgx_bif *b,uint32_t first_context_id)
{
    if (!out || a04_rgx_render_validate(bank,b) || !first_context_id || first_context_id>0xfffffffcU ||
        a04_rgx_fw_alias(out,sizeof(*out),bank->cpu,bank->bytes)) return 1;
    const a04_rgx_render_buffer all[]={b->pc,b->pd,b->pt,b->data};
    for (unsigned i=0;i<4;i++)
        if (a04_rgx_fw_alias(out,sizeof(*out),all[i].cpu,all[i].bytes)) return 1;
    a04_rgx_render_buffer bankCopy=*bank;a04_rgx_bif bifCopy=*b;bank=&bankCopy;b=&bifCopy;
    /* Fixed allocations total <16KiB; preflight above precedes all writes. */
    a04_rgx_fw_zero(bank->cpu,bank->bytes);
    a04_rgx_fw_zero(out,sizeof(*out));out->bank=*bank;out->bif=*b;
    a04_rgx_fw_pool pool={(unsigned char *)bank->cpu,bank->pa,bank->bytes,0,A04_RGX_RENDER_FW_OFFSET};
    out->context=(RGXFWIF_FWRENDERCONTEXT *)a04_rgx_fw_alloc(&pool,sizeof(*out->context),&out->context_addr);
    out->memory=(RGXFWIF_FWMEMCONTEXT *)a04_rgx_fw_alloc(&pool,sizeof(*out->memory),&out->memory_addr);
    out->memory->sPCDevPAddr.uiAddr=b->pc.pa;
    out->memory->uiPageCatBaseRegID=0xffffffffU; /* rgxmem.c INVALID_PCREG, FW allocates slot */
    RGXFWIF_DEV_VIRTADDR ta_state,three_state,rf;
    auto ta=(RGXFWIF_TACTX_STATE *)a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_TACTX_STATE),&ta_state);
    (void)a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_3DCTX_STATE)+4*sizeof(uint32_t),&three_state);
    (void)a04_rgx_fw_alloc(&pool,sizeof(A04_RGX_RF_CMD),&rf); /* flags0 disables reset framework */
    ta->uTAReg_VDM_CALL_STACK_POINTER_Init=b->va; /* owned BIF-mapped scratch, not an input address */
    for (unsigned n=0;n<2;n++) {
        auto c=n?&out->context->s3DContext:&out->context->sTAContext;
        out->control[n]=(RGXFWIF_CCCB_CTL *)a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_CCCB_CTL),&c->psCCBCtl);
        out->commands[n]=(unsigned char *)a04_rgx_fw_alloc(&pool,A04_RGX_CLIENT_BYTES,&c->psCCB);
        out->control[n]->ui32WrapMask=A04_RGX_CLIENT_BYTES-1;
        c->psFWMemContext=out->memory_addr;c->psContextState=n?three_state:ta_state;
        c->psRFCmd=rf;c->eDM=n?3:2;c->ui32Priority=1;
        c->ui32ServerCommonContextID=first_context_id+n;
    }
    out->sync=(volatile uint32_t *)a04_rgx_fw_alloc(&pool,64,&out->sync_addr);
    /* rgxkicksync.c CreateKickSyncContext: independent common context,
     * GP domain, priority0, no suspend state and no reset framework. Do not
     * transplant the GP NULL-task contract into TA/3D contexts. */
    RGXFWIF_DEV_VIRTADDR gp_address;
    out->gp=(RGXFWIF_FWCOMMONCONTEXT *)a04_rgx_fw_alloc(&pool,sizeof(*out->gp),&gp_address);
    auto gp=out->gp;
    out->control[2]=(RGXFWIF_CCCB_CTL *)a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_CCCB_CTL),&gp->psCCBCtl);
    out->commands[2]=(unsigned char *)a04_rgx_fw_alloc(&pool,A04_RGX_CLIENT_BYTES,&gp->psCCB);
    out->control[2]->ui32WrapMask=A04_RGX_CLIENT_BYTES-1;
    gp->psFWMemContext=out->memory_addr;gp->eDM=0;
    gp->ui32ServerCommonContextID=first_context_id+2;
    out->used=pool.used;
    return a04_rgx_bif_build(b);
}
/* Cold/pre-FW publication only. A live MIPS table update additionally needs
 * matching firmware TLB invalidation; this API intentionally does not do it. */
static inline int a04_rgx_render_map_cold(const a04_rgx_render_resources *r,uint32_t *table,unsigned entries,uint64_t table_pa)
{
    if (!r || a04_rgx_render_validate(&r->bank,&r->bif) || !table || entries!=8192 ||
        a04_rgx_fw_alias(table,8192*4,r->bank.cpu,r->bank.bytes) ||
        !a04_rgx_mips_physical_range(table_pa,8192*4,4096) ||
        a04_rgx_fw_overlap(table_pa,8192*4,r->bank.pa,r->bank.bytes)) return 1;
    const a04_rgx_render_buffer list[]={r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data};
    for (unsigned n=0;n<4;n++) if(a04_rgx_fw_alias(table,8192*4,list[n].cpu,list[n].bytes) ||
        a04_rgx_fw_overlap(table_pa,8192*4,list[n].pa,list[n].bytes))return 1;
    for (unsigned n=0;n<16;n++) if(table[A04_RGX_RENDER_FW_OFFSET/4096+n])return 1;
    return a04_rgx_mips_map(table,entries,A04_RGX_RENDER_FW_OFFSET,r->bank.pa,r->bank.bytes,1,0);
}
static constexpr uint32_t a04_rgx_render_align64(uint32_t n) {return (n+63)&~63U;}
enum {
    A04_RENDER_MEM=a04_rgx_render_align64(sizeof(RGXFWIF_FWRENDERCONTEXT)),
    A04_RENDER_TA=a04_rgx_render_align64(A04_RENDER_MEM+sizeof(RGXFWIF_FWMEMCONTEXT)),
    A04_RENDER_3D=a04_rgx_render_align64(A04_RENDER_TA+sizeof(RGXFWIF_TACTX_STATE)),
    A04_RENDER_RF=a04_rgx_render_align64(A04_RENDER_3D+sizeof(RGXFWIF_3DCTX_STATE)+16),
    A04_RENDER_CTL0=a04_rgx_render_align64(A04_RENDER_RF+sizeof(A04_RGX_RF_CMD)),
    A04_RENDER_CCB0=a04_rgx_render_align64(A04_RENDER_CTL0+16),
    A04_RENDER_CTL1=A04_RENDER_CCB0+4096,
    A04_RENDER_CCB1=a04_rgx_render_align64(A04_RENDER_CTL1+16),
    A04_RENDER_SYNC=A04_RENDER_CCB1+4096,
    A04_RENDER_GP=A04_RENDER_SYNC+64,
    A04_RENDER_GPCTL=a04_rgx_render_align64(A04_RENDER_GP+sizeof(RGXFWIF_FWCOMMONCONTEXT)),
    A04_RENDER_GPCCB=a04_rgx_render_align64(A04_RENDER_GPCTL+16),
    A04_RENDER_USED=A04_RENDER_GPCCB+4096
};
static_assert(A04_RENDER_USED<16384,"bounded context bank");
static inline bool a04_rgx_render_view_valid(const a04_rgx_render_resources *r)
{
    if (!r || a04_rgx_render_validate(&r->bank,&r->bif) || r->used!=A04_RENDER_USED) return false;
    auto base=(unsigned char *)r->bank.cpu;
    if ((void *)r->context!=base || (void *)r->memory!=base+A04_RENDER_MEM ||
        (void *)r->control[0]!=base+A04_RENDER_CTL0 || (void *)r->control[1]!=base+A04_RENDER_CTL1 ||
        r->commands[0]!=base+A04_RENDER_CCB0 || r->commands[1]!=base+A04_RENDER_CCB1 ||
        (const void *)r->sync!=base+A04_RENDER_SYNC ||
        r->context_addr.ui32Addr!=0xc0080000 || r->memory_addr.ui32Addr!=0xc0080000+A04_RENDER_MEM ||
        r->sync_addr.ui32Addr!=0xc0080000+A04_RENDER_SYNC) return false;
    if ((void *)r->gp!=base+A04_RENDER_GP || (void *)r->control[2]!=base+A04_RENDER_GPCTL ||
        r->commands[2]!=base+A04_RENDER_GPCCB || r->gp->eDM!=0 ||
        r->gp->psCCBCtl.ui32Addr!=0xc0080000+A04_RENDER_GPCTL ||
        r->gp->psCCB.ui32Addr!=0xc0080000+A04_RENDER_GPCCB ||
        r->gp->psFWMemContext.ui32Addr!=r->memory_addr.ui32Addr ||
        r->gp->psContextState.ui32Addr || r->gp->psRFCmd.ui32Addr) return false;
    if (r->memory->sPCDevPAddr.uiAddr!=r->bif.pc.pa) return false;
    for (unsigned n=0;n<2;n++) {
        auto c=n?&r->context->s3DContext:&r->context->sTAContext;
        if (c->eDM!=(n?3U:2U) ||
            c->psCCBCtl.ui32Addr!=a04_rgx_render_fw_address(r,r->control[n]) ||
            c->psCCB.ui32Addr!=a04_rgx_render_fw_address(r,r->commands[n]) ||
            c->psFWMemContext.ui32Addr!=r->memory_addr.ui32Addr ||
            c->psContextState.ui32Addr!=0xc0080000+(n?A04_RENDER_3D:A04_RENDER_TA) ||
            c->psRFCmd.ui32Addr!=0xc0080000+A04_RENDER_RF) return false;
    }
    return true;
}
static inline unsigned a04_rgx_render_index(unsigned dm) {return dm==0?2U:(dm==3?1U:0U);}
static inline RGXFWIF_FWCOMMONCONTEXT *a04_rgx_render_context(const a04_rgx_render_resources *r,unsigned dm)
{return dm==0?r->gp:(dm==3?&r->context->s3DContext:&r->context->sTAContext);}
struct a04_rgx_render_ring_state {uint32_t write[3],sequence;bool poisoned;};
/* Prepares typed KICK + client bytes, no MMIO. Caller reserves a KCCB slot
 * FIRST under the sole producer lock. Once published, pin resources until
 * actual device shutdown/drain proof. No context/command pointer arguments. */
static inline int a04_rgx_render_update(a04_rgx_render_resources *r,a04_rgx_render_ring_state *state,
                                      unsigned dm,RGXFWIF_KCCB_CMD *kick,uint32_t priority=0xffffffffU)
{
    if (!state || !kick || (dm!=0 && dm!=2 && dm!=3) || !a04_rgx_render_view_valid(r) ||
        state->poisoned || !state->sequence || state->sequence==0xffffffffU) return 1;
    const a04_rgx_render_buffer all[]={r->bank,r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data};
    for (unsigned i=0;i<5;i++)
        if (a04_rgx_fw_alias(kick,sizeof(*kick),all[i].cpu,all[i].bytes) ||
            a04_rgx_fw_alias(state,sizeof(*state),all[i].cpu,all[i].bytes) ||
            a04_rgx_fw_alias(r,sizeof(*r),all[i].cpu,all[i].bytes))return 1;
    if (a04_rgx_fw_alias(kick,sizeof(*kick),state,sizeof(*state)) ||
        a04_rgx_fw_alias(kick,sizeof(*kick),r,sizeof(*r)) ||
        a04_rgx_fw_alias(state,sizeof(*state),r,sizeof(*r))) return 1;
    bool changePriority=priority!=0xffffffffU;
    if (changePriority && ((dm!=2 && dm!=3) || priority>1)) return 1;
    unsigned n=a04_rgx_render_index(dm);auto c=a04_rgx_render_context(r,dm);
    auto ctl=(volatile RGXFWIF_CCCB_CTL *)r->control[n];uint32_t at=state->write[n];
    if (ctl->ui32WrapMask!=4095 || at>4095 || (at&7) || ctl->ui32ReadOffset!=at ||
        ctl->ui32DepOffset>4095 || (ctl->ui32DepOffset&7) || ctl->ui32WriteOffset!=at || 4096-at<40) return 2;
    RGXFWIF_CCB_CMD_HEADER header={};header.eCmdType=A04_RGX_CLIENT_UPDATE;header.ui32CmdSize=8;
    /* rgxfwutils.c ContextSetPriority: aligned8 payload4 =>48 bytes.
     * For this diagnostic constrain input to0/1, require an actual change. */
    if (changePriority && ((volatile RGXFWIF_FWCOMMONCONTEXT *)c)->ui32Priority==priority) return 1;
    unsigned commandBytes=dm==0?88U:48U;
    if (at+commandBytes+40>4096) {
        RGXFWIF_CCB_CMD_HEADER pad={};pad.eCmdType=A04_RGX_CLIENT_PADDING;pad.ui32CmdSize=4096-at-40;
        a04_rgx_fw_zero(r->commands[n]+at,4096-at);
        *(RGXFWIF_CCB_CMD_HEADER *)(r->commands[n]+at)=pad;at=0;
    }
    /* Exact GP-only sync contract from rgxkicksync.c. TA/3D standalone
     * UPDATE helpers remain available for investigation, not boot-executed.
     * No zero-filled/guessed TA/3D task or shader packet is constructed. */
    unsigned updateAt=at;
    if (dm==0) {
        RGXFWIF_CCB_CMD_HEADER nullCmd={};nullCmd.eCmdType=A04_RGX_CLIENT_NULL;
        *(RGXFWIF_CCB_CMD_HEADER *)(r->commands[n]+at)=nullCmd;
        updateAt+=40;
    }
    if (changePriority) header.eCmdType=A04_RGX_CLIENT_PRIORITY;
    *(RGXFWIF_CCB_CMD_HEADER *)(r->commands[n]+updateAt)=header;
    auto ufo=(RGXFWIF_UFO *)(r->commands[n]+updateAt+40);
    if (changePriority) {
        /* Priority payload is4 bytes plus4 zero padding. No UFO write and no
         * UPDATE concatenation: exact ContextSetPriority standalone packet. */
        a04_rgx_fw_zero(ufo,8);
        ((RGXFWIF_CMD_PRIORITY *)ufo)->ui32Priority=priority;
    } else {ufo->puiAddrUFO=r->sync_addr;ufo->ui32Value=state->sequence;*r->sync=0;}
    RGXFWIF_KCCB_CMD cmd={};cmd.eCmdType=RGXFWIF_KCCB_CMD_KICK;cmd.eDM=dm;
    cmd.uCmdData.sCmdKickData.psContext.ui32Addr=a04_rgx_render_fw_address(r,c);
    cmd.uCmdData.sCmdKickData.ui32CWoffUpdate=at+commandBytes;*kick=cmd;
    /* FW owns CCCB_CTL.write; publish ONLY through KCCB.ui32CWoffUpdate. */
    state->write[n]=at+commandBytes;return 0;
}
#endif
