/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded native context UPDATE/UFO or TA/3D PRIORITY via KCCB KICK.
 * This is firmware context execution, NOT rasterization or acceleration.
 * Caller holds the SAME producer/power mutex as GP SYNC and service stop.
 * Kernel runtime and render banks must be normal-NC CPU mappings, all resources
 * already mapped/cleaned before FW start, pinned through any live submission.
 */
#ifndef A04BR3_ROGUE_RENDER_SUBMIT_H
#define A04BR3_ROGUE_RENDER_SUBMIT_H
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueFWRequests.h"
#include "A04BR3RogueRenderResources.h"
#include "A04BR3RogueTransferFill.h"
/* One terminal observation, never a replay/extra poll. Values are a bounded
 * read-only snapshot of concurrently FW-owned words, not an atomic FW dump.
 * No following firmware pointers, no FWCCB consumption, no cache-line clean. */
struct a04_rgx_render_observation {
    bool valid;
    uint32_t dm,polls,kread,kwrite,kexpected,cread,cwrite,cdep,cexpected;
    uint32_t ufo,expected,fread,fwrite,ftype,flags,priority;
    uint32_t runPrev,runNext,waitPrev,waitNext,kexecuted,tracePointer,power,hwr,fwfaults;
    uint32_t startedTime,priorityExpected,prioritySequence,transfer,contextID;
    uint64_t idleTime;
    a04_rgx_fw_requests requests;
};
static inline void a04_rgx_render_observe(a04_rgx_render_observation *out,
    const a04_rgx_render_resources *r,const a04_rgx_render_ring_state *state,
    a04_rgx_fw_runtime *kernel,unsigned dm,unsigned polls,unsigned next,uint32_t priority=0xffffffffU,bool transfer=false)
{
    if (!out || out->valid) return;
    auto client=(volatile RGXFWIF_CCCB_CTL *)r->control[a04_rgx_render_index(dm,transfer)];
    auto context=(volatile RGXFWIF_FWCOMMONCONTEXT *)a04_rgx_render_context(r,dm,transfer);
    out->transfer=transfer;out->dm=dm;out->polls=polls;
    out->kread=kernel->kctl->ui32ReadOffset;out->kwrite=kernel->kctl->ui32WriteOffset;out->kexpected=next;
    out->cread=client->ui32ReadOffset;out->cwrite=client->ui32WriteOffset;out->cdep=client->ui32DepOffset;
    out->cexpected=state->write[a04_rgx_render_index(dm,transfer)];out->ufo=*r->sync;out->expected=state->sequence;
    out->fread=kernel->fctl->ui32ReadOffset;out->fwrite=kernel->fctl->ui32WriteOffset;
    out->ftype=0xffffffffU;out->requests={};
    auto fcommands=(volatile RGXFWIF_FWCCB_CMD *)a04_rgx_command_memory(kernel,
        kernel->init->psFirmwareCCB.ui32Addr,32*sizeof(RGXFWIF_FWCCB_CMD));
    if (fcommands && !a04_rgx_fw_requests_snapshot(&out->requests,sizeof(out->requests),
        (const void *)fcommands,32*sizeof(RGXFWIF_FWCCB_CMD),out->fread,out->fwrite) && out->requests.count)
        out->ftype=out->requests.first[0].type;
    out->contextID=context->ui32ServerCommonContextID;
    out->priorityExpected=priority;out->prioritySequence=context->ui32PrioritySeqNum;
    out->flags=context->ui32FWComCtxFlags;out->priority=context->ui32Priority;
    out->runPrev=context->sRunNode.p.ui32Addr;out->runNext=context->sRunNode.n.ui32Addr;
    out->waitPrev=context->sWaitingNode.p.ui32Addr;out->waitNext=context->sWaitingNode.n.ui32Addr;
    auto trace=(volatile RGXFWIF_TRACEBUF *)a04_rgx_command_memory(kernel,
        kernel->init->sTraceBufCtl.ui32Addr,sizeof(RGXFWIF_TRACEBUF));
    if (trace && (const void *)trace==kernel->trace) {
        out->kexecuted=trace->ui32KCCBCmdsExecuted;out->tracePointer=trace->sTraceBuf[0].ui32TracePointer;
        out->power=trace->ePowState;out->hwr=trace->ui32HWRStateFlags;
        out->fwfaults=trace->ui32FWFaults;out->idleTime=trace->ui64StartIdleTime;
    }
    out->startedTime=kernel->init->ui32FirmwareStartedTimeStamp;
    out->valid=true;
}
static inline int a04_rgx_render_submit_update(a04_rgx_render_resources *r,
    a04_rgx_render_ring_state *state,a04_rgx_fw_runtime *kernel,
    const a04_rgx_command_io *io,unsigned dm,a04_rgx_render_observation *observation=nullptr,uint32_t priority=0xffffffffU,bool transfer=false,const a04_rgx_tq_clear *clear=nullptr)
{
    if (!r || !state || !kernel || !io || !io->acquire || !io->publish || !io->kick ||
        !io->wait_us || !kernel->init || !a04_rgx_render_view_valid(r) ||
        (dm!=0 && dm!=2 && dm!=3) || state->sequence>=0xfffffffeU) return A04_CCB_INPUT;
    if (clear && (!transfer || dm!=3 || priority!=0xffffffffU)) return A04_CCB_INPUT;
    if (transfer && (dm!=3 || (priority==0xffffffffU && !clear))) return A04_CCB_INPUT;
    bool changePriority=priority!=0xffffffffU;
    if (changePriority && ((dm!=2 && dm!=3) || priority>1)) return A04_CCB_INPUT;
    if (state->poisoned) return A04_CCB_OFFLINE;
    if ((void *)kernel->init!=kernel->config.cpu || kernel->config.bytes!=A04_RGX_CONFIG_BYTES ||
        kernel->init->bFirmwareStarted!=IMG_TRUE || kernel->init->sRGXCompChecks.bUpdated!=IMG_TRUE)
        return A04_CCB_OFFLINE;
    const auto &compat=kernel->init->sRGXCompChecks;
    if (compat.ui32DDKVersion!=0x1000b || compat.ui32DDKBuild!=5425693 || compat.ui32FWProcessorVersion!=1 ||
        compat.sFWBVNC.ui32LayoutVersion!=3 || compat.sFWBVNC.ui64BVNC!=0x001600280036001eULL ||
        compat.sHWBVNC.ui64BVNC!=0x001600280036001eULL) return A04_CCB_OFFLINE;
    auto ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(kernel,kernel->init->psKernelCCBCtl.ui32Addr,16);
    auto commands=(RGXFWIF_KCCB_CMD *)a04_rgx_command_memory(kernel,kernel->init->psKernelCCB.ui32Addr,128*56);
    auto fctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(kernel,kernel->init->psFirmwareCCBCtl.ui32Addr,16);
    if (!ctl || ctl!=kernel->kctl || !commands || commands!=kernel->kcommands ||
        !fctl || fctl!=kernel->fctl) return A04_CCB_INPUT;
    const a04_rgx_render_buffer parts[]={r->bank,r->bif.pc,r->bif.pd,r->bif.pt,r->bif.data};
    a04_rgx_render_buffer runtime={kernel->main.cpu,kernel->main.physical,kernel->main.bytes};
    a04_rgx_render_buffer config={kernel->config.cpu,kernel->config.physical,kernel->config.bytes};
    for (unsigned i=0;i<5;i++)
        if (a04_rgx_render_buffers_overlap(parts[i],runtime) ||
            a04_rgx_render_buffers_overlap(parts[i],config) ||
            a04_rgx_fw_alias(state,sizeof(*state),parts[i].cpu,parts[i].bytes)) return A04_CCB_INPUT;
    if (a04_rgx_fw_alias(state,sizeof(*state),runtime.cpu,runtime.bytes) ||
        a04_rgx_fw_alias(state,sizeof(*state),config.cpu,config.bytes)) return A04_CCB_INPUT;
    if (observation) {
        if (!a04_rgx_fw_pointer(observation,sizeof(*observation)) ||
            a04_rgx_fw_alias(observation,sizeof(*observation),state,sizeof(*state)) ||
            a04_rgx_fw_alias(observation,sizeof(*observation),r,sizeof(*r)) ||
            a04_rgx_fw_alias(observation,sizeof(*observation),kernel,sizeof(*kernel)) ||
            a04_rgx_fw_alias(observation,sizeof(*observation),runtime.cpu,runtime.bytes) ||
            a04_rgx_fw_alias(observation,sizeof(*observation),config.cpu,config.bytes)) return A04_CCB_INPUT;
        for (unsigned i=0;i<5;i++)
            if (a04_rgx_fw_alias(observation,sizeof(*observation),parts[i].cpu,parts[i].bytes)) return A04_CCB_INPUT;
    }
    io->acquire(io->context);
    uint32_t read=ctl->ui32ReadOffset,write=ctl->ui32WriteOffset,next=(write+1)&127;
    if (ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 || read>127 || write>127 ||
        fctl->ui32WrapMask!=31 || fctl->ui32CmdSize!=40 ||
        fctl->ui32ReadOffset>31 || fctl->ui32WriteOffset>31) {
        state->poisoned=true;return A04_CCB_CORRUPT;
    }
    if (next==read) return A04_CCB_FULL;
    if (read!=write) return A04_CCB_BUSY;
    if (fctl->ui32ReadOffset!=fctl->ui32WriteOffset) return A04_CCB_FW_REQUEST;
    RGXFWIF_KCCB_CMD kick;
    ++state->sequence;
    int error=clear?a04_rgx_tq_clear_enqueue(r,state,clear,&kick):
        a04_rgx_render_update(r,state,dm,&kick,priority,transfer);
    if (error) {--state->sequence;return error==2?A04_CCB_BUSY:A04_CCB_INPUT;}
    commands[write]=kick;
    io->publish(io->context);
    ctl->ui32WriteOffset=next;(void)ctl->ui32WriteOffset;
    io->publish(io->context);
    io->kick(io->context,0xb00,0x10);
    auto client=(volatile RGXFWIF_CCCB_CTL *)r->control[a04_rgx_render_index(dm,transfer)];
    for (unsigned i=0;i<10000;i++) {
        io->acquire(io->context);
        uint32_t value=*r->sync,kread=ctl->ui32ReadOffset,cread=client->ui32ReadOffset;
        if (kread>127 || cread>4095 || (cread&7) || (!changePriority && value && value!=state->sequence) ||
            ctl->ui32WriteOffset!=next || ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 ||
            client->ui32WrapMask!=4095 || client->ui32WriteOffset>4095 ||
            (client->ui32WriteOffset&7) || client->ui32DepOffset>4095 || (client->ui32DepOffset&7)) {a04_rgx_render_observe(observation,r,state,kernel,dm,i+1,next,priority,transfer);state->poisoned=true;return A04_CCB_CORRUPT;}
        auto context=(volatile RGXFWIF_FWCOMMONCONTEXT *)a04_rgx_render_context(r,dm,transfer);
        bool response=changePriority?context->ui32Priority==priority:value==state->sequence;
        if (response && kread==next && cread==state->write[a04_rgx_render_index(dm,transfer)] &&
            client->ui32WriteOffset==state->write[a04_rgx_render_index(dm,transfer)]) return A04_CCB_OK;
        io->wait_us(io->context,10);
    }
    io->acquire(io->context);
    a04_rgx_render_observe(observation,r,state,kernel,dm,10000,next,priority,transfer);
    state->poisoned=true;return A04_CCB_TIMEOUT;
}
#endif
