/* SPDX-License-Identifier: MIT
 * Late MT8167 inherited-watchdog quiescence; see build-support/CREDITS.md.
 */
#include <pexpert/arm64/board_config.h>
#if defined(A04BR3)
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <pexpert/arm/protos.h>
#include <machine/machine_routines.h>
#include <machine/atomic.h>
#include <kern/debug.h>
void a04br3_watchdog_quiesce(void)
{
    vm_offset_t base = ml_io_map(0x10007000, 0x1000);
    if (!base) { panic("A04BR3 WDT: mapping failed"); }
    volatile uint32_t *mode = (volatile uint32_t *)base;
    uint32_t before = *mode;
    *mode = (before & ~1u) | 0x22000000u;
    __builtin_arm_dsb(DSB_SY);
    uint32_t after = *mode;
    kprintf("A04BR3 WDT: late quiesce mode %08x -> %08x\n", before, after);
    if (after & 1u) { panic("A04BR3 WDT: enable bit did not clear"); }
}
#endif
