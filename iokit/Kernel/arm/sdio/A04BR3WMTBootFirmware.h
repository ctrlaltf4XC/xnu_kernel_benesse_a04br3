/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Payload v5 CPU-readable WMT view only. GPU remains the final reservation.
 * This does not parse/send firmware or borrow GPU storage. Caller must pass the
 * returned view to the SDIO-owned snapshot/parser after HVR/FVR are known.
 */
#ifndef A04BR3_WMT_BOOT_FIRMWARE_H
#define A04BR3_WMT_BOOT_FIRMWARE_H
#include <stdint.h>
#include <stddef.h>
static inline int a04_wmt_boot_firmware_range(uint64_t pa,uint64_t bytes,
    uint64_t gpuPA,uint64_t gpuBytes,uint64_t kernelBase,uint64_t kernelBytes,
    uint64_t physBase,uint64_t memSize,uint64_t top)
{
    const uint64_t page=0x4000,limit=0x100000000ULL;
    if(bytes<32 || bytes>32ULL*1024*1024 || gpuBytes<=4096 || gpuBytes>32ULL*1024*1024 ||
        (pa&(page-1)) || (gpuPA&(page-1)) || !physBase || !memSize ||
        memSize>limit || physBase>limit-memSize || kernelBase!=physBase ||
        !kernelBytes || kernelBytes>memSize || (kernelBytes&(page-1)))return 0;
    uint64_t end=kernelBase+kernelBytes;
    uint64_t wmtRounded=(bytes+page-1)&~(page-1),gpuRounded=(gpuBytes+page-1)&~(page-1);
    if(top<end || top-end!=2*page || top>physBase+memSize || pa<=kernelBase ||
        pa>end || wmtRounded>end-pa || pa+wmtRounded!=gpuPA ||
        gpuPA>end || gpuRounded>end-gpuPA || gpuPA+gpuRounded!=end)return 0;
    return 1;
}
#ifdef __cplusplus
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include <pexpert/arm64/boot.h>
extern "C" vm_offset_t ml_static_ptovirt(vm_offset_t);
enum { A04_WMT_BOOT_OK=0,A04_WMT_BOOT_ABSENT=1,A04_WMT_BOOT_RANGE=2,A04_WMT_BOOT_MAPPING=3 };
static inline int a04_wmt_boot_firmware_view(const void **view,unsigned *bytes)
{
    if(!view || !bytes)return A04_WMT_BOOT_RANGE;
    *view=nullptr;*bytes=0;
    DTEntry map=nullptr;
    if(SecureDTLookupEntry(nullptr,"/chosen/memory-map",&map)!=kSuccess)return A04_WMT_BOOT_ABSENT;
    const char *keys[]={"WMTFirmware-A04BR3","GPUFirmware-A04BR3","Kernel-A04BR3"};
    uint64_t pairs[3][2]={};
    for(unsigned i=0;i<3;i++){
        const void *value=nullptr;unsigned size=0;
        if(SecureDTGetProperty(map,keys[i],&value,&size)!=kSuccess)return i?A04_WMT_BOOT_RANGE:A04_WMT_BOOT_ABSENT;
        if(!value || size!=sizeof(pairs[i]))return A04_WMT_BOOT_RANGE;
        __builtin_memcpy(pairs[i],value,sizeof(pairs[i]));
    }
    const void *rd=nullptr;unsigned rdBytes=0;
    if(SecureDTGetProperty(map,"RAMDisk",&rd,&rdBytes)==kSuccess)return A04_WMT_BOOT_RANGE;
    const boot_args *ba=(const boot_args *)PE_state.bootArgs;
    if(!ba || !a04_wmt_boot_firmware_range(pairs[0][0],pairs[0][1],pairs[1][0],pairs[1][1],
        pairs[2][0],pairs[2][1],ba->physBase,ba->memSize,ba->topOfKernelData))return A04_WMT_BOOT_RANGE;
    vm_offset_t source=ml_static_ptovirt((vm_offset_t)pairs[0][0]);
    uint64_t length=pairs[0][1];
    if(!source || source>UINTPTR_MAX-(length-1))return A04_WMT_BOOT_MAPPING;
    /* Check every mapped page, not just endpoints: no hidden interior hole. */
    for(uint64_t off=0x4000;off<length;off+=0x4000)
        if(ml_static_ptovirt((vm_offset_t)(pairs[0][0]+off))!=source+off)return A04_WMT_BOOT_MAPPING;
    if(ml_static_ptovirt((vm_offset_t)(pairs[0][0]+length-1))!=source+length-1)return A04_WMT_BOOT_MAPPING;
    *view=(const void *)source;*bytes=(unsigned)length;return A04_WMT_BOOT_OK;
}
#endif
#endif
