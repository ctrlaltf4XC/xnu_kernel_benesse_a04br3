/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded native render-context client UPDATE -> KCCB KICK -> UFO response.
 * This is firmware context execution, NOT rasterization or acceleration.
 * Caller holds the SAME producer/power mutex as GP SYNC and service stop.
 * Kernel runtime and render banks must be normal-NC CPU mappings, all resources
 * already mapped/cleaned before FW start, pinned through any live submission.
 */
#ifndef A04BR3_ROGUE_RENDER_SUBMIT_H
#define A04BR3_ROGUE_RENDER_SUBMIT_H
#include "A04BR3RogueCommand.h"
#include "A04BR3RogueRenderResources.h"
static inline int a04_rgx_render_submit_update(a04_rgx_render_resources *r,
    a04_rgx_render_ring_state *state,a04_rgx_fw_runtime *kernel,
    const a04_rgx_command_io *io,unsigned dm)
{
    if (!r || !state || !kernel || !io || !io->acquire || !io->publish || !io->kick ||
        !io->wait_us || !kernel->init || !a04_rgx_render_view_valid(r) ||
        (dm!=2 && dm!=3) || state->sequence>=0xfffffffeU) return A04_CCB_INPUT;
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
    int error=a04_rgx_render_update(r,state,dm,&kick);
    if (error) {--state->sequence;return error==2?A04_CCB_BUSY:A04_CCB_INPUT;}
    commands[write]=kick;
    io->publish(io->context);
    ctl->ui32WriteOffset=next;(void)ctl->ui32WriteOffset;
    io->publish(io->context);
    io->kick(io->context,0xb00,0x10);
    auto client=(volatile RGXFWIF_CCCB_CTL *)r->control[dm==3];
    for (unsigned i=0;i<10000;i++) {
        io->acquire(io->context);
        uint32_t value=*r->sync,kread=ctl->ui32ReadOffset,cread=client->ui32ReadOffset;
        if (kread>127 || cread>4095 || (cread&7) || (value && value!=state->sequence) ||
            ctl->ui32WriteOffset!=next || ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 ||
            client->ui32WrapMask!=4095 || client->ui32WriteOffset>4095 ||
            (client->ui32WriteOffset&7) || client->ui32DepOffset>4095 || (client->ui32DepOffset&7)) {state->poisoned=true;return A04_CCB_CORRUPT;}
        if (value==state->sequence && kread==next && cread==state->write[dm==3]) return A04_CCB_OK;
        io->wait_us(io->context,10);
    }
    state->poisoned=true;return A04_CCB_TIMEOUT;
}
#endif
