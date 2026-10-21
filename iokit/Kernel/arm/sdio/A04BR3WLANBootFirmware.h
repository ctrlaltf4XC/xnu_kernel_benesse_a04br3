/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * v6 raw MTKW input precedes the validated WMT+GPU reservation tail.
 * Returns a CPU view only; caller copies/validates before device transfer.
 */
#ifndef A04BR3_WLAN_BOOT_FIRMWARE_H
#define A04BR3_WLAN_BOOT_FIRMWARE_H
#include "A04BR3WMTBootFirmware.h"
static inline int a04_wlan_boot_firmware_range(const uint64_t p[4][2],
    uint64_t physical,uint64_t memory,uint64_t top)
{
    if (!p || p[0][1]<32 || p[0][1]>16ULL*1024*1024 ||
        (p[0][0]&0x3fff) || p[0][0]<=physical ||
        !a04_wmt_boot_firmware_range(p[1][0],p[1][1],p[2][0],p[2][1],
            p[3][0],p[3][1],physical,memory,top)) return 0;
    uint64_t rounded=(p[0][1]+0x3fff)&~0x3fffULL;
    return p[0][0]<p[1][0] && rounded==p[1][0]-p[0][0];
}
#ifdef __cplusplus
enum { A04_WLAN_BOOT_OK=0,A04_WLAN_BOOT_ABSENT=1,A04_WLAN_BOOT_RANGE=2,A04_WLAN_BOOT_MAPPING=3 };
static inline int a04_wlan_boot_firmware_view(const void **view,unsigned *bytes)
{
    if (!view || !bytes) return A04_WLAN_BOOT_RANGE;
    *view=nullptr;*bytes=0;
    DTEntry map=nullptr;
    if (SecureDTLookupEntry(nullptr,"/chosen/memory-map",&map)!=kSuccess) return A04_WLAN_BOOT_ABSENT;
    const char *keys[]={"WLANFirmware-A04BR3","WMTFirmware-A04BR3","GPUFirmware-A04BR3","Kernel-A04BR3"};
    uint64_t pairs[4][2]={};
    for (unsigned i=0;i<4;i++) {
        const void *value=nullptr;unsigned size=0;
        if (SecureDTGetProperty(map,keys[i],&value,&size)!=kSuccess)
            return i?A04_WLAN_BOOT_RANGE:A04_WLAN_BOOT_ABSENT;
        if (!value || size!=sizeof(pairs[i])) return A04_WLAN_BOOT_RANGE;
        __builtin_memcpy(pairs[i],value,sizeof(pairs[i]));
    }
    const void *rd=nullptr;unsigned rdBytes=0;
    if (SecureDTGetProperty(map,"RAMDisk",&rd,&rdBytes)==kSuccess) return A04_WLAN_BOOT_RANGE;
    const boot_args *ba=(const boot_args *)PE_state.bootArgs;
    if (!ba || !a04_wlan_boot_firmware_range(pairs,ba->physBase,ba->memSize,ba->topOfKernelData))
        return A04_WLAN_BOOT_RANGE;
    vm_offset_t source=ml_static_ptovirt((vm_offset_t)pairs[0][0]);
    uint64_t length=pairs[0][1];
    if (!source || source>UINTPTR_MAX-(length-1)) return A04_WLAN_BOOT_MAPPING;
    for (uint64_t off=0x4000;off<length;off+=0x4000)
        if (ml_static_ptovirt((vm_offset_t)(pairs[0][0]+off))!=source+off) return A04_WLAN_BOOT_MAPPING;
    if (ml_static_ptovirt((vm_offset_t)(pairs[0][0]+length-1))!=source+length-1) return A04_WLAN_BOOT_MAPPING;
    *view=(const void *)source;*bytes=(unsigned)length;
    return A04_WLAN_BOOT_OK;
}
#endif
#endif
