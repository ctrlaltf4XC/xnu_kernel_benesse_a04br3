/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pure startup transcript builder, NOT an MMIO executor or permission to boot.
 * Profile: BVNC 22.40.54.30, MIPS/32bit PA/40bit VA, non-S7, nonsecure,
 * non-shared 64KiB SLC/512bit line, AXI-ACE, BRNs 60084/61450/63553.
 * Source: m1.11ED5425693 rgxstartstop.c RGXStart/RGXInitMipsProcWrapper,
 * rgxlayer_impl.c, rgxmipsmmuinit.c, rgxinit.c IRQ/AllocTrampoline,
 * rgxfwutils.c RGXSetupFirmware and rgxpower.c RGXPostPowerState.
 * No I/O callbacks, hardware access, firmware bytes or fabricated FW ABI.
 * All evidence flags are caller obligations, not evidence discovered here.
 * REG_APERTURE requires access through at least core+0xa103 (0x1000 is not
 * enough); BUILD_POLICY requires nonsecure/nonshared/S7-absent assumptions.
 */
#ifndef A04BR3_ROGUE_STARTUP_PLAN_H
#define A04BR3_ROGUE_STARTUP_PLAN_H
#include <stdint.h>
#include <stddef.h>
#include "A04BR3RogueMipsMMU.h"
enum a04_rgx_start_evidence {
    A04_START_CPU_STAGED=1U, A04_START_POWER_CLOCK=2U,
    A04_START_FULL_FW_MAPPINGS=4U, A04_START_INIT_ABI=8U,
    A04_START_CCB_RUNTIME=16U, A04_START_CACHE_SYNC=32U,
    A04_START_TRAMPOLINE=64U, A04_START_IRQ_ROUTE=128U,
    A04_START_BUILD_POLICY=256U, A04_START_REG_APERTURE=512U, A04_START_ALL=1023U
};
enum a04_rgx_start_kind {
    A04_START_WRITE32=1, A04_START_WRITE64, A04_START_FENCE32,
    A04_START_FENCE64, A04_START_RMW32, A04_START_RMW64,
    A04_START_WAIT_US, A04_START_POLL_SHARED32, A04_START_READ_IRQ,
    A04_START_BLOCKED
};
/* RMW semantics: (old & keep_mask) | value. WAIT keep_mask records DDK
 * cycle count; POLL keep_mask is comparison mask. No operation is executed. */
struct a04_rgx_start_op {
    uint32_t kind, address;
    uint64_t value, keep_mask;
};
struct a04_rgx_start_input {
    uint64_t bvnc, code_pa, data_pa, table_pa, trampoline_pa;
    /* Caller supplies real ABI offsetof(bFirmwareStarted), never a guessed
     * offset. Shared init allocation must also be mapped in firmware table. */
    uint64_t init_pa;
    /* init_bytes is the page-rounded owned allocation, not sizeof(FWIF). */
    uint32_t init_bytes, started_offset, ready_initial, evidence;
    uint32_t strict_same_address_ordering, device_pa0_valid;
    uint32_t poll_budget, poll_delay_us;
};
struct a04_rgx_start_plan {
    uint32_t count, missing, execution_allowed, poll_budget, poll_delay_us;
    struct a04_rgx_start_op ops[32];
};
enum { A04_START_OK=0, A04_START_BAD_INPUT=1, A04_START_INCOMPLETE=2 };
static inline int a04_rgx_start_overlap(uint64_t a,uint32_t n,uint64_t b,uint32_t m)
{
    return a<b+m && b<a+n;
}
static inline void a04_rgx_start_add(struct a04_rgx_start_plan *p,uint32_t kind,
                                    uint32_t address,uint64_t value,uint64_t keep)
{
    /* Private helper: the fixed builder below emits at most 28 operations. */
    struct a04_rgx_start_op *op=&p->ops[p->count++];
    op->kind=kind;op->address=address;op->value=value;op->keep_mask=keep;
}
static inline int a04_rgx_start_plan(const struct a04_rgx_start_input *input,
                                     struct a04_rgx_start_plan *out)
{
    if (!input || !out) return A04_START_BAD_INPUT;
    const struct a04_rgx_start_input x=*input;
    if (x.bvnc!=0x001600280036001eULL || (x.evidence&~A04_START_ALL) ||
        x.strict_same_address_ordering!=1 || x.device_pa0_valid>1 ||
        !x.poll_budget || x.poll_budget>1000000 || !x.poll_delay_us || x.poll_delay_us>1000000)
        return A04_START_BAD_INPUT;
    uint64_t pa[5]={x.code_pa,x.data_pa,x.table_pa,x.trampoline_pa,x.init_pa};
    uint32_t sizes[5]={0x19000,0x5000,0x8000,0x4000,x.init_bytes};
    for (unsigned i=0;i<5;i++) {
        if (i>=3 && !pa[i]) continue; /* absent optional allocation => blocked */
        if (!a04_rgx_mips_physical_range(pa[i],sizes[i],i==2?0x8000:(i==3?0x4000:0x1000)))
            return A04_START_BAD_INPUT;
        /* Physical 0x1fc00000..0x1fc03fff is sensitive to wrapper remaps.
         * Conservatively reject any owned allocation in that window. */
        if (a04_rgx_start_overlap(pa[i],sizes[i],0x1fc00000,0x4000)) return A04_START_BAD_INPUT;
        for (unsigned j=0;j<i;j++)
            if (pa[j] && a04_rgx_start_overlap(pa[i],sizes[i],pa[j],sizes[j]))
                return A04_START_BAD_INPUT;
    }
    if (x.init_pa && ((x.started_offset&3) || x.init_bytes<4 ||
        x.started_offset>x.init_bytes-4)) return A04_START_BAD_INPUT;
    struct a04_rgx_start_plan plan;
    unsigned char *zero=(unsigned char *)&plan;
    for (size_t i=0;i<sizeof(plan);i++) zero[i]=0;
    plan.missing=A04_START_ALL&~x.evidence;
    if (!x.trampoline_pa) plan.missing|=A04_START_TRAMPOLINE;
    if (!x.init_pa || x.ready_initial!=0) plan.missing|=A04_START_INIT_ABI;
    plan.poll_budget=x.poll_budget;plan.poll_delay_us=x.poll_delay_us;
    /* Complete concrete prerequisites permit the owning executor to proceed.
     * This pure helper itself never discovers evidence or touches hardware. */
    plan.execution_allowed=plan.missing?0:1;
    const uint64_t reset=0x00e7fffffffffc1dULL;
    const uint64_t rascal_dusts=0x000001f8e0000000ULL;
    a04_rgx_start_add(&plan,A04_START_WRITE32,0xa100,0,0); /* sys_bus_secure */
    a04_rgx_start_add(&plan,A04_START_FENCE32,0xa100,0,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x100,reset,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x100,reset^rascal_dusts,0);
    a04_rgx_start_add(&plan,A04_START_FENCE64,0x100,0,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x100,0x100000000ULL,0); /* hold MIPS */
    /* EnableClocks in this DDK leaves automatic gating at hardware defaults.
     * BRN61450 bypasses IPF_OBJ/CPF; 64KiB is below TPU's 128KiB threshold.
     * This DDK defines strict same-address ordering; without ERN61389,
     * BRN60084 preserves the existing PSG hazard bit rather than forcing it.
     * Write only low32 of SLC_MISC to retain high32 scramble defaults. */
    a04_rgx_start_add(&plan,A04_START_RMW64,0x3828,0x03008000,~0x03008000ULL);
    a04_rgx_start_add(&plan,A04_START_RMW32,0x3800,
        0x200001U,2);
    /* 32bit PA wrapper uses physical register bank, NOT 0xcf400000 VA. */
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x810,(0x13000000U>>16)|0x10000U,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x818,0x1fc00001,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x820,(x.code_pa+0x18000)|12,0);
    /* BRN63553: on 32bit cores remap PA0 only when it is NOT a valid PA. */
    if (!x.device_pa0_valid) {
        a04_rgx_start_add(&plan,A04_START_WRITE64,0x858,1,0);
        a04_rgx_start_add(&plan,A04_START_WRITE64,0x860,(x.code_pa+0x18000)|12,0);
    }
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x828,0x1fc01001,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x830,(x.data_pa+0x3000)|12,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x838,0x1fc02001,0);
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x840,(x.code_pa+0x17000)|12,0);
    if (x.trampoline_pa) {
        a04_rgx_start_add(&plan,A04_START_WRITE64,0x848,x.trampoline_pa|1,0);
        a04_rgx_start_add(&plan,A04_START_WRITE64,0x850,0x1fc00000|14,0);
    }
    a04_rgx_start_add(&plan,A04_START_WRITE64,0xb50,0,0); /* idle controlled by MIPS */
    a04_rgx_start_add(&plan,A04_START_WRITE32,0x8c8,0,0); /* EJTAG probe */
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x38c0,0x8882af00ULL,0); /* AXI-ACE */
    /* MIPS RGXInitBIF has NO catalogue-base write (nonsecure profile).
     * Table PA is delivered in boot4words; not a BIF_CAT_BASE0 register. */
    if (plan.missing) {
        a04_rgx_start_add(&plan,A04_START_BLOCKED,0,plan.missing,0);
        *out=plan;return A04_START_INCOMPLETE;
    }
    a04_rgx_start_add(&plan,A04_START_WAIT_US,0,3,32); /* DDK 32 cycles / 3us */
    a04_rgx_start_add(&plan,A04_START_WRITE64,0x100,0,0); /* master microMIPS boot */
    a04_rgx_start_add(&plan,A04_START_FENCE64,0x100,0,0);
    a04_rgx_start_add(&plan,A04_START_WAIT_US,0,3,32);
    a04_rgx_start_add(&plan,A04_START_POLL_SHARED32,(uint32_t)(x.init_pa+x.started_offset),1,0xffffffffU);
    a04_rgx_start_add(&plan,A04_START_READ_IRQ,0x8a8,0,1);
    *out=plan;return A04_START_OK;
}

/* Pure observation reducer. Caller invalidates/synchronizes the shared CPU
 * cache line before each sample and supplies a previously observed zero flag.
 * IRQ event is independent of readiness; stale 1/all-ones/IRQ-only cannot pass.
 * Exception status is diagnostic, not blindly treated as fatal (ERL at reset).
 * This helper never acknowledges any IRQ; pending bit needs a separate W1C. */
struct a04_rgx_start_watch {
    uint32_t remaining, armed_zero, ready, irq_pending, exception_status;
};
static inline int a04_rgx_start_sample(struct a04_rgx_start_watch *watch,
    uint32_t firmware_started,uint32_t irq_status,uint32_t exception_status,int cache_synced)
{
    if (!watch || cache_synced!=1 || watch->armed_zero!=1) return A04_START_BAD_INPUT;
    if (!watch->ready && !watch->remaining) return A04_START_INCOMPLETE;
    watch->irq_pending=irq_status&1;
    watch->exception_status=exception_status;
    if (watch->ready) return A04_START_OK; /* boot milestone, not health monitor */
    watch->remaining--;
    if (firmware_started==1) { watch->ready=1;return A04_START_OK; }
    return A04_START_INCOMPLETE;
}
static inline uint32_t a04_rgx_start_irq_clear_value(uint32_t irq_status)
{
    /* rgxinit.c: status at 0x08a8; WRITE32(0x08b0,1) only for event bit. */
    return irq_status&1;
}
#endif
