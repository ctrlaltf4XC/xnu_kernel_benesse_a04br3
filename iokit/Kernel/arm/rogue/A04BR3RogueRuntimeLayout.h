/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source-derived minimal native MIPS firmware runtime, no rendering contexts.
 * ABI declarations: Imagination/MediaTek MIT DDK m1.11ED5425693, vendored in
 * A04BR3RogueDDKABI.h. Initialization follows rgxfwutils.c RGXSetupFirmware/
 * RGXSetupOSConfig/RGXSetupCCB; heap partition follows rgx_heap_firmware.h.
 */
#ifndef A04BR3_ROGUE_RUNTIME_LAYOUT_H
#define A04BR3_ROGUE_RUNTIME_LAYOUT_H
#include "A04BR3RogueDDKABI.h"
#include "A04BR3RogueFirmware.h"
#include "A04BR3RogueMipsMMU.h"
#ifdef __cplusplus
static_assert(sizeof(RGXFWIF_INIT)==592 && offsetof(RGXFWIF_INIT,bFirmwareStarted)==260,"DDK ABI drift");
#endif
enum { A04_RGX_MAIN_BYTES=0x40000,A04_RGX_CONFIG_BYTES=0x10000,
       A04_RGX_MAIN_OFFSET=0x40000,A04_RGX_CONFIG_OFFSET=0x1ff0000 };
struct a04_rgx_fw_pool {
    unsigned char *cpu;uint64_t physical;uint32_t bytes,used,heap_offset;
};
struct a04_rgx_fw_runtime {
    struct a04_rgx_fw_pool main,config;
    RGXFWIF_INIT *init;
    RGXFWIF_TRACEBUF *trace;
    RGXFWIF_RUNTIME_CFG *runtime;
    RGXFWIF_OS_CONFIG *osconfig;
    RGXFWIF_HWRINFOBUF *hwr;
    RGXFWIF_CCB_CTL *kctl,*fctl;
    RGXFWIF_KCCB_CMD *kcommands;
    uint32_t *power_sync;
    uint32_t *command_sync;
    RGXFWIF_DEV_VIRTADDR command_sync_addr;
};
static inline void *a04_rgx_fw_alloc(struct a04_rgx_fw_pool *pool,uint32_t bytes,
                                    RGXFWIF_DEV_VIRTADDR *firmware)
{
    if (!pool || !bytes || !firmware || pool->used>pool->bytes) return NULL;
    uint32_t at=(pool->used+63U)&~63U;
    if (at<pool->used || at>pool->bytes || bytes>pool->bytes-at) return NULL;
    firmware->ui32Addr=0xc0000000U+pool->heap_offset+at;
    pool->used=at+bytes;
    return pool->cpu+at;
}
static inline int a04_rgx_fw_ccb(struct a04_rgx_fw_pool *pool,
    PRGXFWIF_CCB_CTL *control,PRGXFWIF_CCB *commands,uint32_t count,uint32_t command_size,
    RGXFWIF_CCB_CTL **host)
{
    if (!count || (count&(count-1)) || !command_size || count>4096 || command_size>4096) return 1;
    *host=(RGXFWIF_CCB_CTL *)a04_rgx_fw_alloc(pool,sizeof(**host),control);
    if (!*host || !a04_rgx_fw_alloc(pool,count*command_size,commands)) return 1;
    (*host)->ui32WrapMask=count-1;(*host)->ui32CmdSize=command_size;
    return 0;
}
static inline int a04_rgx_fw_runtime_init(struct a04_rgx_fw_runtime *out,
    void *main_cpu,uint64_t main_pa,void *config_cpu,uint64_t config_pa,uint32_t clock_hz)
{
    if (!out || !a04_rgx_fw_pointer(main_cpu,A04_RGX_MAIN_BYTES) ||
        !a04_rgx_fw_pointer(config_cpu,A04_RGX_CONFIG_BYTES) ||
        ((uintptr_t)main_cpu&7) || ((uintptr_t)config_cpu&7) ||
        !a04_rgx_mips_physical_range(main_pa,A04_RGX_MAIN_BYTES,4096) ||
        !a04_rgx_mips_physical_range(config_pa,A04_RGX_CONFIG_BYTES,4096) ||
        a04_rgx_fw_overlap(main_pa,A04_RGX_MAIN_BYTES,config_pa,A04_RGX_CONFIG_BYTES) ||
        a04_rgx_fw_alias(main_cpu,A04_RGX_MAIN_BYTES,config_cpu,A04_RGX_CONFIG_BYTES) ||
        clock_hz<1000000 || clock_hz>1000000000) return 1;
    a04_rgx_fw_zero(out,sizeof(*out));
    a04_rgx_fw_zero(main_cpu,A04_RGX_MAIN_BYTES);
    a04_rgx_fw_zero(config_cpu,A04_RGX_CONFIG_BYTES);
    out->main.cpu=(unsigned char *)main_cpu;out->main.physical=main_pa;
    out->main.bytes=A04_RGX_MAIN_BYTES;out->main.heap_offset=A04_RGX_MAIN_OFFSET;
    out->config.cpu=(unsigned char *)config_cpu;out->config.physical=config_pa;
    out->config.bytes=A04_RGX_CONFIG_BYTES;out->config.heap_offset=A04_RGX_CONFIG_OFFSET;
    RGXFWIF_DEV_VIRTADDR address;
    out->init=(RGXFWIF_INIT *)a04_rgx_fw_alloc(&out->config,sizeof(RGXFWIF_INIT),&address);
    /* FwInitStructure MUST be the first native config allocation. */
    if (!out->init || address.ui32Addr!=0xc1ff0000U) return 2;
    RGXFWIF_INIT *init=out->init;
    out->trace=(RGXFWIF_TRACEBUF *)a04_rgx_fw_alloc(&out->main,sizeof(*out->trace),&init->sTraceBufCtl);
    out->hwr=(RGXFWIF_HWRINFOBUF *)a04_rgx_fw_alloc(&out->main,sizeof(*out->hwr),&init->sRGXFWIfHWRInfoBufCtl);
    out->runtime=(RGXFWIF_RUNTIME_CFG *)a04_rgx_fw_alloc(&out->main,sizeof(*out->runtime),&init->sRuntimeCfg);
    out->osconfig=(RGXFWIF_OS_CONFIG *)a04_rgx_fw_alloc(&out->main,sizeof(*out->osconfig),&init->sOSConfig);
    if (!out->trace || !out->hwr || !out->runtime || !out->osconfig) return 2;
    out->power_sync=(uint32_t *)a04_rgx_fw_alloc(&out->main,4,&out->osconfig->sPowerSync);
    if (!out->power_sync) return 2;
    out->osconfig->sTraceBufCtl=init->sTraceBufCtl;
    out->osconfig->sRGXFWIfHWRInfoBufCtl=init->sRGXFWIfHWRInfoBufCtl;
    /* First FW-ready experiment: HWR enabled, PDP disabled, no rendering kicks. */
    out->osconfig->ui32ConfigFlags=(1U<<7)|(1U<<29);
    if (a04_rgx_fw_ccb(&out->main,&init->psKernelCCBCtl,&init->psKernelCCB,128,
                      sizeof(RGXFWIF_KCCB_CMD),&out->kctl) ||
        a04_rgx_fw_ccb(&out->main,&init->psFirmwareCCBCtl,&init->psFirmwareCCB,32,
                      sizeof(RGXFWIF_FWCCB_CMD),&out->fctl)) return 2;
    out->kcommands=(RGXFWIF_KCCB_CMD *)(out->main.cpu+
        init->psKernelCCB.ui32Addr-(0xc0000000U+A04_RGX_MAIN_OFFSET));
    RGXFWIF_CCB_CTL *checkpoint;
    if (a04_rgx_fw_ccb(&out->main,&init->psCheckpointCCBCtl,&init->psCheckpointCCB,1024,
                      sizeof(PRGXFWIF_UFO_ADDR),&checkpoint)) return 2;
    /* Runtime trace buffer is real storage; host pointer is host-only in DDK. */
    void *trace=a04_rgx_fw_alloc(&out->main,12000*4,&address);
    if (!trace) return 2;
    out->trace->ui32LogType=3; /* TRACE|MAIN, source enum values */
    out->trace->ui32TraceBufSizeInDWords=12000;
    out->trace->sTraceBuf[0].pui32RGXFWIfTraceBuffer=address;
    out->trace->sTraceBuf[0].pui32TraceBuffer=(IMG_PUINT32)trace;
    RGXFWIF_GPU_UTIL_FWCB *util=(RGXFWIF_GPU_UTIL_FWCB *)a04_rgx_fw_alloc(
        &out->main,sizeof(*util),&init->sGpuUtilFWCbCtl);
    if (!util) return 2;
    util->ui64LastWord=1; /* IDLE; no fabricated timestamp */
    if (!a04_rgx_fw_alloc(&out->main,sizeof(RGXFWIF_HWPERF_CTL),&init->sHWPerfCtl)) return 2;
    uint32_t *clock=(uint32_t *)a04_rgx_fw_alloc(&out->main,4,&init->sCoreClockRate);
    if (!clock) return 2;
    *clock=clock_hz;
    /* Reserved fault page, naturally 4KiB aligned and fully mapped. */
    out->main.used=(out->main.used+4095U)&~4095U;
    if (!a04_rgx_fw_alloc(&out->main,4096,&address)) return 2;
    init->sFaultPhysAddr.uiAddr=main_pa+address.ui32Addr-(0xc0000000U+A04_RGX_MAIN_OFFSET);
    /* Separate owned UFO, never reuse the firmware power-management sync. */
    out->command_sync=(uint32_t *)a04_rgx_fw_alloc(&out->main,64,&out->command_sync_addr);
    if (!out->command_sync) return 2;
    /* BRN65273 is present on 22.40.54.30. */
    init->sPDSExecBase.uiAddr=0xa800000000ULL;init->sUSCExecBase.uiAddr=0xba00000000ULL;
    init->ui32HWRDebugDumpLimit=99999;
    init->ui32InitialCoreClockSpeed=clock_hz;
    init->ui32ActivePMLatencyms=100;
    init->ui32MarkerVal=1;
    init->sRGXCompChecks.sFWBVNC.ui32LayoutVersion=3;
    init->sRGXCompChecks.sHWBVNC.ui32LayoutVersion=3;
    init->eBifTilingMode=0; /* rgx_mtk/sysconfig.c 256x16 */
    for (unsigned i=0;i<4;i++) {
        init->sBifTilingCfg[i].uiBase=0xd000000000ULL+0x200000000ULL*i;
        init->sBifTilingCfg[i].uiLen=0x200000000ULL;
        init->sBifTilingCfg[i].uiXStride=i; /* MTK sysconfig.h strides0,1,2,3 */
    }
    out->runtime->ui32ActivePMLatencyms=100;
    out->runtime->ui32CoreClockSpeed=clock_hz;
    out->runtime->ui32DefaultDustsNumInit=1;
    /* Compatible empty signature/HWPerf filters, META-only fields and optional
     * workload/GCOV/register config are zero (disabled by matching user config). */
    return 0;
}
static inline int a04_rgx_fw_runtime_map(uint32_t *table,unsigned entries,
                                        uint64_t main_pa,uint64_t config_pa)
{
    if (!table || entries!=8192) return 1;
    /* Preflight BOTH ranges before either write; no partial map on conflict. */
    for (unsigned i=0;i<A04_RGX_MAIN_BYTES/4096;i++)
        if (table[A04_RGX_MAIN_OFFSET/4096+i]) return 1;
    for (unsigned i=0;i<A04_RGX_CONFIG_BYTES/4096;i++)
        if (table[A04_RGX_CONFIG_OFFSET/4096+i]) return 1;
    if (!a04_rgx_mips_physical_range(main_pa,A04_RGX_MAIN_BYTES,4096) ||
        !a04_rgx_mips_physical_range(config_pa,A04_RGX_CONFIG_BYTES,4096)) return 1;
    return a04_rgx_mips_map(table,entries,A04_RGX_MAIN_OFFSET,main_pa,A04_RGX_MAIN_BYTES,1,0) ||
           a04_rgx_mips_map(table,entries,A04_RGX_CONFIG_OFFSET,config_pa,A04_RGX_CONFIG_BYTES,1,0);
}
#endif
