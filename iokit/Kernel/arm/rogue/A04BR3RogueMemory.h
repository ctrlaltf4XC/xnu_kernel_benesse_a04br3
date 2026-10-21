/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Inert host memory reservation. No GPU address is installed in hardware.
 * CPU-only reservation/staging for the validated 22.40.54.30 layout.
 * The GPU never receives these addresses and firmware is never executed here.
 */
#ifndef A04BR3_ROGUE_MEMORY_H
#define A04BR3_ROGUE_MEMORY_H
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <arm/caches_internal.h>
#include "A04BR3RogueMipsMMU.h"
#include "A04BR3RogueFirmware.h"
class A04BR3RogueMemory {
    IOBufferMemoryDescriptor *buffers[3] = {};
    bool prepared[3] = {};
    bool staged = false;
    static unsigned bytes(unsigned i) {
        return i==0 ? TableBytes : (i==1 ? CodeBytes : DataBytes);
    }
    /* Prepared descriptors must still describe disjoint, contiguous, owned
     * host physical memory; never use a supplied PA or IOMapper address. */
    bool snapshot(void *cpu[3],uint64_t pa[3]) {
        for (unsigned i=0;i<3;i++) {
            if (!buffers[i] || !prepared[i]) return false;
            IOByteCount length=0;
            pa[i]=buffers[i]->getPhysicalSegment(0,&length);
            cpu[i]=buffers[i]->getBytesNoCopy();
            if (!a04_rgx_mips_physical_range(pa[i],bytes(i),i?4096:TableBytes) ||
                length<bytes(i) || !a04_rgx_fw_pointer(cpu[i],bytes(i)) ||
                (i==0 && ((uintptr_t)cpu[i]&3))) return false;
            for (unsigned j=0;j<i;j++) {
                if (a04_rgx_fw_overlap(pa[i],bytes(i),pa[j],bytes(j)) ||
                    a04_rgx_fw_alias(cpu[i],bytes(i),cpu[j],bytes(j))) return false;
            }
        }
        return true;
    }
    static bool matchesProfile(const a04_rgx_fw_layout &layout) {
        /* DDK rgxinit.c reserves MAXIMUM code+exceptions+boot size before
         * data, including dummy pages. Actual code size 0x19000 is NOT the
         * data heap offset: 0x2f000+0x2000+0x1000 = 0x32000.
         * Exact section profile below is from the validated vendor info v1. */
        static const a04_rgx_fw_section expected[6]={
            {4,1,0xc0000000U,0x2f000,0x17000,0},
            {5,1,0x9fc02000U,0x2000,0x1000,0x17000},
            {6,1,0xbfc00000U,0x1000,0x1000,0x18000},
            {7,2,0xc0032000U,0x8000,0x3000,0},
            {8,2,0xbfc01000U,0x1000,0x1000,BootDataOffset},
            {9,2,0xcf600000U,0x1000,0x1000,StackOffset}
        };
        if (layout.code_bytes!=CodeBytes || layout.data_bytes!=DataBytes ||
            layout.entry_point!=0xbfc00001U) return false;
        for (unsigned i=0;i<6;i++) {
            const a04_rgx_fw_section &a=layout.sections[i],&b=expected[i];
            if (a.id!=b.id || a.kind!=b.kind || a.base!=b.base ||
                a.maximum!=b.maximum || a.allocated!=b.allocated ||
                a.offset!=b.offset) return false;
        }
        unsigned codeSections=0;bool fileBackedEntry=false;
        for (unsigned i=0;i<layout.segment_count;i++) {
            const a04_rgx_fw_segment &s=layout.segments[i];
            if (s.permissions!=(s.kind==1 ? 5U : 6U)) return false;
            if (s.kind==1 && s.filesz) {
                codeSections |= s.target<0x17000 ? 1U : (s.target<0x18000 ? 2U : 4U);
                uint32_t entry=layout.entry_point&~1U;
                if (entry>=s.address && (uint64_t)entry<(uint64_t)s.address+s.filesz)
                    fileBackedEntry=true;
            }
        }
        return codeSections==7 && fileBackedEntry;
    }
    /* Keep the validation layout off stage()'s stack across unpack(), which
     * has its own bounded parser scratch. Otherwise three layouts can be
     * live on the kernel stack at once. No persistent parser state is kept. */
    static __attribute__((noinline)) int validateImage(const void *image,size_t size) {
        a04_rgx_fw_layout layout;
        if (a04_rgx_fw_parse(image,size,&layout)) return StageImage;
        return matchesProfile(layout) ? StageOK : StageProfile;
    }
    int failStage(int error) { reset(); return error; }
    A04BR3RogueMemory(const A04BR3RogueMemory &) = delete;
    A04BR3RogueMemory &operator=(const A04BR3RogueMemory &) = delete;
public:
    enum { TableBytes=32768, CodeBytes=102400, DataBytes=20480,
           CodeHeapOffset=0, DataHeapOffset=0x32000,
           BootDataOffset=0x3000, StackOffset=0x4000 };
    enum StageResult { StageOK=0, StageNoMemory=1, StageImage=2,
                       StageProfile=3, StageAddress=4, StageAlias=5,
                       StageUnpack=6, StageTable=7 };
    A04BR3RogueMemory() = default;
    ~A04BR3RogueMemory() { reset(); }
    void reset() {
        staged=false;
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
        void *cpu[3];uint64_t pa[3];
        if (!snapshot(cpu,pa)) { reset();return false; }
        __asm__ volatile("dsb sy":::"memory");
        return true;
    }
    /* Means CPU buffers were staged/cache-cleaned, NOT firmware running or
     * safe to start. Caller must serialize reserve/reset/stage and keep the
     * external blob immutable for the entire call. This is format/profile
     * validation, not cryptographic authentication of a payload delivery.
     * Any failed stage releases ALL buffers and clears the staged state;
     * reserve() is required before retry. No stale candidate remains usable. */
    bool firmwareStaged() const { return staged; }
    int stage(const void *image,size_t imageBytes) {
        staged=false;
        for (unsigned i=0;i<3;i++)
            if (!buffers[i] || !prepared[i]) return failStage(StageNoMemory);
        void *cpu[3];uint64_t pa[3];
        if (!snapshot(cpu,pa)) return failStage(StageAddress);
        /* Reject aliasing before parse or any code/data write. The parser
         * independently validates size/span and the unpacker checks again. */
        if (imageBytes<=A04_RGX_FW_INFO_BYTES || imageBytes>A04_RGX_FW_LIMIT ||
            !a04_rgx_fw_pointer(image,imageBytes)) return failStage(StageImage);
        for (unsigned i=0;i<3;i++)
            if (a04_rgx_fw_alias(image,imageBytes,cpu[i],bytes(i)))
                return failStage(StageAlias);
        int validation=validateImage(image,imageBytes);
        if (validation) return failStage(validation);
        if (a04_rgx_fw_unpack(image,imageBytes,cpu[1],CodeBytes,cpu[2],DataBytes,NULL))
            return failStage(StageUnpack);
        a04_rgx_fw_zero(cpu[0],TableBytes);
        if (a04_rgx_mips_map((uint32_t *)cpu[0],TableBytes/4,CodeHeapOffset,
                             pa[1],CodeBytes,0,1) ||
            a04_rgx_mips_map((uint32_t *)cpu[0],TableBytes/4,DataHeapOffset,
                             pa[2],DataBytes,1,0)) return failStage(StageTable);
        /* rgxfwimageutils.c / rgx_mips.h: four little-endian uint64 words
         * at MIPS_BOOT_DATA + 0. Stack argument is PAGE BASE, not stack top. */
        const uint64_t boot[4]={0x13000000ULL,pa[0],pa[2]+StackOffset,0};
        unsigned char *target=(unsigned char *)cpu[2]+BootDataOffset;
        for (unsigned i=0;i<4;i++)
            for (unsigned n=0;n<8;n++) target[i*8+n]=(unsigned char)(boot[i]>>(n*8));
        /* Finish all CPU writes before publishing a staged flag. Explicit
         * ARM flushing avoids coherent-IO build shortcuts; table last. */
        flush_dcache((vm_offset_t)cpu[1],CodeBytes,0);
        flush_dcache((vm_offset_t)cpu[2],DataBytes,0);
        flush_dcache((vm_offset_t)cpu[0],TableBytes,0);
        __asm__ volatile("dsb sy":::"memory");
        staged=true;
        return StageOK;
    }
};
#endif
