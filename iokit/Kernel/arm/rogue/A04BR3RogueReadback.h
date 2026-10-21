/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GPU SLC -> CPU readback boundary, matching MIT DDK m1.11ED5425693.
 * rgxinit.c RGXFreeUFOBlock: global SLCFLUSHINVAL (bInval=TRUE,
 * bDMContext=FALSE, context=0, eDM=GP), then RGXWaitForFWOp on GP.
 * rgxcompute.c PVRSRVRGXFlushComputeDataKM also explicitly waits for an FW
 * SYNC after a flush; client UFO/CCB completion is not an SLC flush proof.
 * Exact ABI/copyright retained in A04BR3RogueDDKABI.h. Original implementation.
 * Sole producer/power lock MUST remain held across task, flush/SYNC, readback.
 * Both commands are published once as an ordered GP batch, no replay/MMIO
 * cache manipulation. Existing 100ms per-operation bound, not an extended
 * deadline for the rendering task. CPU output mapping MUST be normal-NC.
 */
#ifndef A04BR3_ROGUE_READBACK_H
#define A04BR3_ROGUE_READBACK_H
#include "A04BR3RogueCommand.h"
struct a04_rgx_readback_observation {
    uint32_t polls,kread,kwrite,kexpected,ufo,expected,fread,fwrite;
    bool valid,submitted;
};
static inline int a04_rgx_command_readback(a04_rgx_fw_runtime *r,
    a04_rgx_command_state *state,const a04_rgx_command_io *io,
    a04_rgx_readback_observation *out)
{
    if(!r || !state || !io || !out || !io->publish || !io->acquire || !io->kick || !io->wait_us ||
       !r->init || state->sequence==0xffffffffU)return A04_CCB_INPUT;
    if(state->poisoned)return A04_CCB_OFFLINE;
    if((void *)r->init!=r->config.cpu || r->config.bytes!=A04_RGX_CONFIG_BYTES ||
       r->init->bFirmwareStarted!=IMG_TRUE || r->init->sRGXCompChecks.bUpdated!=IMG_TRUE)return A04_CCB_OFFLINE;
    const auto &compat=r->init->sRGXCompChecks;
    if(compat.ui32DDKVersion!=0x1000b || compat.ui32DDKBuild!=5425693 || compat.ui32FWProcessorVersion!=1 ||
       compat.sFWBVNC.ui32LayoutVersion!=3 || compat.sFWBVNC.ui64BVNC!=0x001600280036001eULL ||
       compat.sHWBVNC.ui64BVNC!=0x001600280036001eULL)return A04_CCB_OFFLINE;
    auto ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psKernelCCBCtl.ui32Addr,16);
    auto cmds=(RGXFWIF_KCCB_CMD *)a04_rgx_command_memory(r,r->init->psKernelCCB.ui32Addr,128*56);
    auto sync=(volatile uint32_t *)a04_rgx_command_memory(r,r->command_sync_addr.ui32Addr,64);
    auto fctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psFirmwareCCBCtl.ui32Addr,16);
    if(!ctl || ctl!=r->kctl || !cmds || cmds!=r->kcommands || !sync || sync!=r->command_sync ||
       !fctl || fctl!=r->fctl ||
       a04_rgx_fw_alias(state,sizeof(*state),r->main.cpu,r->main.bytes) ||
       a04_rgx_fw_alias(state,sizeof(*state),r->config.cpu,r->config.bytes) ||
       a04_rgx_fw_alias(out,sizeof(*out),r->main.cpu,r->main.bytes) ||
       a04_rgx_fw_alias(out,sizeof(*out),r->config.cpu,r->config.bytes) ||
       a04_rgx_fw_alias(out,sizeof(*out),state,sizeof(*state)) ||
       a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)))return A04_CCB_INPUT;
    io->acquire(io->context);
    uint32_t read=ctl->ui32ReadOffset,write=ctl->ui32WriteOffset,next=(write+2)&127;
    if(ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56 || read>127 || write>127 ||
       fctl->ui32WrapMask!=31 || fctl->ui32CmdSize!=40 ||
       fctl->ui32ReadOffset>31 || fctl->ui32WriteOffset>31){state->poisoned=true;return A04_CCB_CORRUPT;}
    if(fctl->ui32ReadOffset!=fctl->ui32WriteOffset){
        /* No flush/SYNC was issued. Preserve real blocked ring coordinates;
         * zero-filled observations are not evidence that either ring is0. */
        *out={};out->kread=read;out->kwrite=write;out->kexpected=write;
        out->ufo=*sync;out->expected=state->sequence;
        out->fread=fctl->ui32ReadOffset;out->fwrite=fctl->ui32WriteOffset;out->valid=true;
        return A04_CCB_FW_REQUEST;
    }
    /* One producer, require an empty ring; two slots fit even across127->0. */
    if(read!=write)return A04_CCB_BUSY;
    uint32_t sequence=++state->sequence;
    *sync=0;
    RGXFWIF_KCCB_CMD flush={},fence={};
    flush.eCmdType=RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;flush.eDM=A04_RGX_DM_GP;
    flush.uCmdData.sSLCFlushInvalData.bInval=IMG_TRUE;
    flush.uCmdData.sSLCFlushInvalData.bDMContext=IMG_FALSE;
    flush.uCmdData.sSLCFlushInvalData.eDM=A04_RGX_DM_GP;
    flush.uCmdData.sSLCFlushInvalData.psContext.ui32Addr=0;
    fence.eCmdType=RGXFWIF_KCCB_CMD_SYNC;fence.eDM=A04_RGX_DM_GP;
    fence.uCmdData.sSyncData.sSyncObjDevVAddr=r->command_sync_addr;
    fence.uCmdData.sSyncData.uiUpdateVal=sequence;
    cmds[write]=flush;cmds[(write+1)&127]=fence;
    io->publish(io->context);ctl->ui32WriteOffset=next;(void)ctl->ui32WriteOffset;
    io->publish(io->context);io->kick(io->context,0xb00,0x10);
    int result=A04_CCB_TIMEOUT;unsigned polls=0;
    for(unsigned n=0;n<10000;n++){
        polls=n+1;io->acquire(io->context);
        uint32_t value=*sync,done=ctl->ui32ReadOffset;
        if(done>127 || (value && value!=sequence) || ctl->ui32WriteOffset!=next ||
           ctl->ui32WrapMask!=127 || ctl->ui32CmdSize!=56){result=A04_CCB_CORRUPT;break;}
        if(value==sequence && done==next){result=A04_CCB_OK;break;}
        io->wait_us(io->context,10);
    }
    io->acquire(io->context);
    out->submitted=true;out->polls=polls;out->kread=ctl->ui32ReadOffset;out->kwrite=ctl->ui32WriteOffset;out->kexpected=next;
    out->ufo=*sync;out->expected=sequence;out->fread=fctl->ui32ReadOffset;out->fwrite=fctl->ui32WriteOffset;out->valid=true;
    if(result==A04_CCB_TIMEOUT || result==A04_CCB_CORRUPT)state->poisoned=true;
    return result;
}
static_assert(offsetof(RGXFWIF_KCCB_CMD,uCmdData)==16,"native KCCB union offset");
static_assert(sizeof(RGXFWIF_SLCFLUSHINVALDATA)==16 &&
    offsetof(RGXFWIF_SLCFLUSHINVALDATA,bInval)==4 &&
    offsetof(RGXFWIF_SLCFLUSHINVALDATA,bDMContext)==8 &&
    offsetof(RGXFWIF_SLCFLUSHINVALDATA,eDM)==12,"native SLC flush ABI");
static_assert((unsigned)RGXFWIF_KCCB_CMD_SLCFLUSHINVAL==0x2abc0069U,"native SLC flush opcode");
#endif
