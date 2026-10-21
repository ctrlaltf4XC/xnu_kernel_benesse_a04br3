/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Inert host memory reservation. No GPU address is installed in hardware.
 * Firmware code/data sizes are the validated 22.40.54.30 layout, not a loader.
 */
#ifndef A04BR3_ROGUE_MEMORY_H
#define A04BR3_ROGUE_MEMORY_H
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <arm/caches_internal.h>
#include "A04BR3RogueMipsMMU.h"
class A04BR3RogueMemory {
    IOBufferMemoryDescriptor *buffers[3] = {};
    bool prepared[3] = {};
    A04BR3RogueMemory(const A04BR3RogueMemory &) = delete;
    A04BR3RogueMemory &operator=(const A04BR3RogueMemory &) = delete;
public:
    enum { TableBytes=32768, CodeBytes=102400, DataBytes=20480 };
    A04BR3RogueMemory() = default;
    ~A04BR3RogueMemory() { reset(); }
    void reset() {
        for (unsigned i=0;i<3;i++) {
            if (buffers[i]) {
                if (prepared[i]) buffers[i]->complete();
                buffers[i]->release(); buffers[i]=nullptr;
            }
            prepared[i]=false;
        }
    }
    bool reserve() {
        reset();
        const unsigned sizes[3]={TableBytes,CodeBytes,DataBytes};
        for (unsigned i=0;i<3;i++) {
            /* A single contiguous table, aligned to its 32KiB size. All bus
             * addresses are host physical addresses: bypass any IOMMU mapper. */
            buffers[i]=IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
                kernel_task,kIODirectionInOut|kIOMemoryPhysicallyContiguous|
                kIOMemoryMapperNone,sizes[i],i ? 0xfffff000ULL : 0xffff8000ULL);
            if (!buffers[i] || buffers[i]->prepare()!=kIOReturnSuccess) {
                reset();return false;
            }
            prepared[i]=true;
            IOByteCount length=0;
            uint64_t pa=buffers[i]->getPhysicalSegment(0,&length);
            if (!a04_rgx_mips_physical_range(pa,sizes[i],i?4096:TableBytes) ||
                length<sizes[i] || !buffers[i]->getBytesNoCopy()) {
                reset();return false;
            }
            bzero(buffers[i]->getBytesNoCopy(),sizes[i]);
            /* Use the explicit ARM cache primitive: performOperation may be
             * a no-op on builds declaring coherent I/O, unlike this GPU. */
            flush_dcache((vm_offset_t)buffers[i]->getBytesNoCopy(),sizes[i],0);
        }
        __asm__ volatile("dsb sy":::"memory");
        return true;
    }
};
#endif
