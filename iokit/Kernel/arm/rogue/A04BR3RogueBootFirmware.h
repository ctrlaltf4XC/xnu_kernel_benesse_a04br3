/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * v4 CPU-only firmware asset reservation contract, before any PA translation.
 * Kernel-A04BR3 covers kernel+asset. ADT and boot_args follow in two 16KiB
 * pages below topOfKernelData, outside the asset's rounded reservation.
 */
#ifndef A04BR3_ROGUE_BOOT_FIRMWARE_H
#define A04BR3_ROGUE_BOOT_FIRMWARE_H
#include <stdint.h>
static inline int a04_rgx_boot_firmware_range(uint64_t pa,uint64_t length,
    uint64_t kernelBase,uint64_t kernelBytes,uint64_t physBase,uint64_t memSize,
    uint64_t top)
{
    const uint64_t page=0x4000;
    if (length<=4096 || length>32ULL*1024*1024 || (pa&(page-1)) ||
        !physBase || !memSize || memSize>0x100000000ULL ||
        physBase>0x100000000ULL-memSize || kernelBase!=physBase ||
        !kernelBytes || kernelBytes>memSize || (kernelBytes&(page-1))) return 0;
    uint64_t end=kernelBase+kernelBytes,rounded=(length+page-1)&~(page-1);
    if (top<end || top-end!=2*page || top>physBase+memSize ||
        pa<=kernelBase || pa>end || rounded>end-pa || pa+rounded!=end) return 0;
    return 1;
}
#endif
