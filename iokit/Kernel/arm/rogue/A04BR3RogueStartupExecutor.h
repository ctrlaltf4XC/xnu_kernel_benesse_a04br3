/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded DDK startup executor. Only a fully validated matching plan is accepted.
 * MMIO and cache maintenance are supplied by the owning kernel transport.
 */
#ifndef A04BR3_ROGUE_STARTUP_EXECUTOR_H
#define A04BR3_ROGUE_STARTUP_EXECUTOR_H
#include "A04BR3RogueStartupPlan.h"
struct a04_rgx_exec_io {
    void *context;
    uint32_t (*read32)(void *,uint32_t);
    uint64_t (*read64)(void *,uint32_t);
    void (*write32)(void *,uint32_t,uint32_t);
    void (*write64)(void *,uint32_t,uint64_t);
    void (*wait_us)(void *,uint32_t);
    /* Must invalidate FW-written shared cache line and read with acquire
     * barriers. Address is physical; callback validates owned init allocation. */
    uint32_t (*shared32)(void *,uint32_t);
};
enum { A04_EXEC_OK=0,A04_EXEC_INPUT=1,A04_EXEC_BUS=2,A04_EXEC_TIMEOUT=3 };
static inline int a04_rgx_execute_start(const struct a04_rgx_start_input *input,
    const struct a04_rgx_exec_io *io,uint32_t *irq_status)
{
    struct a04_rgx_start_plan plan;
    if (!input || (uint64_t)input->poll_budget*input->poll_delay_us>1000000 ||
        !io || !io->read32 || !io->read64 || !io->write32 || !io->write64 ||
        !io->wait_us || !io->shared32 || !irq_status ||
        a04_rgx_start_plan(input,&plan)!=A04_START_OK) return A04_EXEC_INPUT;
    /* Ready must be observed zero before even the first reset/wrapper write. */
    uint32_t ready_pa=(uint32_t)(input->init_pa+input->started_offset);
    if (io->shared32(io->context,ready_pa)!=0) return A04_EXEC_INPUT;
    for (unsigned i=0;i<plan.count;i++) {
        const struct a04_rgx_start_op *op=&plan.ops[i];
        switch (op->kind) {
        case A04_START_WRITE32:io->write32(io->context,op->address,(uint32_t)op->value);break;
        case A04_START_WRITE64:io->write64(io->context,op->address,op->value);break;
        case A04_START_RMW32: {
            uint32_t old=io->read32(io->context,op->address);
            if (old==0xffffffffU) return A04_EXEC_BUS;
            io->write32(io->context,op->address,(old&(uint32_t)op->keep_mask)|(uint32_t)op->value);
            break;
        }
        case A04_START_RMW64: {
            uint64_t old=io->read64(io->context,op->address);
            if (old==~0ULL) return A04_EXEC_BUS;
            io->write64(io->context,op->address,(old&op->keep_mask)|op->value);
            break;
        }
        case A04_START_FENCE32:
            if (io->read32(io->context,op->address)==0xffffffffU) return A04_EXEC_BUS;
            break;
        case A04_START_FENCE64:
            if (io->read64(io->context,op->address)==~0ULL) return A04_EXEC_BUS;
            break;
        case A04_START_WAIT_US:io->wait_us(io->context,(uint32_t)op->value);break;
        case A04_START_POLL_SHARED32: {
            unsigned poll=0;
            for (;poll<plan.poll_budget;poll++) {
                uint32_t value=io->shared32(io->context,op->address);
                if (value==1) break;
                if (value!=0) return A04_EXEC_BUS;
                io->wait_us(io->context,plan.poll_delay_us);
            }
            if (poll==plan.poll_budget) return A04_EXEC_TIMEOUT;
            break;
        }
        case A04_START_READ_IRQ:
            *irq_status=io->read32(io->context,op->address);
            if (*irq_status==0xffffffffU) return A04_EXEC_BUS;
            break;
        default:return A04_EXEC_INPUT;
        }
    }
    return A04_EXEC_OK;
}
#endif
