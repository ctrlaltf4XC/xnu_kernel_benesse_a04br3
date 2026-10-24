/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 descriptor format adapted from the existing BEGONIA EDK2
 * SdCardBlockIoDxe (BEGONIA_EMMC_BUILD). Private contiguous bounce buffer;
 * no arbitrary user physical addresses are accepted.
 */
#ifndef BEGONIA_MSDC_DMA_H
#define BEGONIA_MSDC_DMA_H
struct begonia_dma_gpd { uint32_t info,next,pointer,length,argument,blocks,command; };
struct begonia_dma_bd { uint32_t info,next,pointer,length; };
_Static_assert(sizeof(struct begonia_dma_gpd)==28,"GPD layout");
_Static_assert(sizeof(struct begonia_dma_bd)==16,"BD layout");
static uint8_t begonia_dma_checksum(const void *descriptor)
{
    const uint8_t *p=descriptor;unsigned sum=0;
    for(unsigned i=0;i<16;i++)sum+=p[i];
    return (uint8_t)(255u-sum);
}
static int begonia_dma_descriptors(struct begonia_dma_gpd *gpd,
    struct begonia_dma_bd *bd,uint64_t gpd_pa,uint64_t bd_pa,
    uint64_t buffer_pa,unsigned bytes)
{
    /* All managed BEGONIA DRAM is below 4GiB. Explicitly reject truncation. */
    if(!bytes || bytes>65536 || (bytes&511u) || (gpd_pa&15u) || (bd_pa&15u) ||
        (buffer_pa&63u) || gpd_pa>0xffffffffULL-55 ||
        bd_pa>0xffffffffULL-15 || buffer_pa>0x100000000ULL-bytes)return -1;
    memset(gpd,0,2*sizeof(*gpd));memset(bd,0,sizeof(*bd));
    bd->info=1;bd->pointer=(uint32_t)buffer_pa;bd->length=bytes;
    bd->info|=(uint32_t)begonia_dma_checksum(bd)<<8;
    gpd[0].info=3;gpd[0].next=(uint32_t)(gpd_pa+sizeof(*gpd));
    gpd[0].pointer=(uint32_t)bd_pa;
    gpd[0].info|=(uint32_t)begonia_dma_checksum(gpd)<<8;
    gpd[1].info=(uint32_t)begonia_dma_checksum(gpd+1)<<8;
    return 0;
}
#endif
