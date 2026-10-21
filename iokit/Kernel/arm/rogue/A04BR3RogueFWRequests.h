/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only bounded FWCCB terminal snapshot. Exact matching DDK declarations
 * and their original copyright/license live in A04BR3RogueDDKABI.h.
 * rgx_fwif_km.h CONTEXT_RESET_NOTIFICATION is105, NOT the KCCB SLC opcode
 * despite identical numeric values. rgxfwutils.c RGXCheckFirmwareCCB stores
 * reason/job by server context ID. This observer never consumes/ACKs a slot,
 * follows a firmware pointer, performs recovery or authorizes further work.
 */
#ifndef A04BR3_ROGUE_FW_REQUESTS_H
#define A04BR3_ROGUE_FW_REQUESTS_H
#include "A04BR3RogueDDKABI.h"
#include "A04BR3RogueFirmware.h"
struct a04_rgx_fw_request {
    uint32_t slot,type,flags,context,reason,job,pageFault;
    uint64_t pc,fault;
    bool reset;
};
struct a04_rgx_fw_requests {
    uint32_t read,write,pending,count;
    a04_rgx_fw_request first[2];
    bool valid;
};
static inline int a04_rgx_fw_requests_snapshot(a04_rgx_fw_requests *out,size_t out_bytes,
    const void *ring,size_t ring_bytes,uint32_t read,uint32_t write)
{
    const size_t bytes=32U*40U;
    if(out_bytes<sizeof(*out) || ring_bytes<bytes || read>=32 || write>=32 ||
       !a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
       !a04_rgx_fw_pointer(ring,bytes) || a04_rgx_fw_alias(out,sizeof(*out),ring,bytes))return 1;
    a04_rgx_fw_requests made={};made.read=read;made.write=write;
    made.pending=(write-read)&31U;made.count=made.pending<2?made.pending:2;
    for(unsigned n=0;n<made.count;n++){
        auto &v=made.first[n];v.slot=(read+n)&31U;
        const volatile uint8_t *p=(const volatile uint8_t *)ring+v.slot*40;
        uint8_t local[40];for(unsigned b=0;b<40;b++)local[b]=p[b];
        v.type=a04_rgx_fw_u32(local);v.flags=a04_rgx_fw_u32(local+4);
        v.reset=v.type==(uint32_t)RGXFWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION;
        if(v.reset){
            v.context=a04_rgx_fw_u32(local+8);v.reason=a04_rgx_fw_u32(local+12);
            v.job=a04_rgx_fw_u32(local+16);v.pageFault=a04_rgx_fw_u32(local+20);
            v.pc=a04_rgx_fw_u64(local+24);v.fault=a04_rgx_fw_u64(local+32);
        }
    }
    made.valid=true;*out=made;return 0;
}
static_assert(sizeof(RGXFWIF_FWCCB_CMD)==40 && offsetof(RGXFWIF_FWCCB_CMD,uCmdData)==8,"FWCCB40 union8");
static_assert(sizeof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA)==32 &&
    offsetof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA,eResetReason)==4 &&
    offsetof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA,ui32ResetJobRef)==8 &&
    offsetof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA,bPageFault)==12 &&
    offsetof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA,ui64PCAddress)==16 &&
    offsetof(RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA,sFaultAddress)==24,"reset payload fields");
static_assert((uint32_t)RGXFWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION==0x2abc0069U,"FW context reset opcode");
#endif
