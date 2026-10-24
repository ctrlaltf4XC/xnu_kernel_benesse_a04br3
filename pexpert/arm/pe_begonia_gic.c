/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 yuma shimizu
 * Redmi Note 8 Pro (begonia), MT6785 / Helio G90T.
 * ARM GICv3 interrupt controller bring-up for boot CPU.
 * Secure Group 0 ownership is left to ATF/EL3. Non-secure Group 1 only.
 */
#include <pexpert/arm64/board_config.h>
#if defined(BEGONIA)
#include <stdint.h>
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <kern/debug.h>
#include <machine/machine_routines.h>
#include <machine/atomic.h>
#include <arm64/proc_reg.h>
#include <pexpert/device_tree.h>
#include <pexpert/arm/protos.h>
static vm_offset_t gicd_base, gicr_base, gicc_base;
static volatile uint32_t ipi_count, timer_count;
static uint32_t cpu_targets[MAX_CPUS];
static uint32_t read_reg(vm_offset_t base, unsigned offset)
{
    return *(volatile uint32_t *)(base + offset);
}
static void write_reg(vm_offset_t base, unsigned offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}
void begonia_gic_init(void)
{
    if (!gicd_base) {
        DTEntry node;
        const void *property;
        unsigned int bytes;
        if (SecureDTLookupEntry(NULL, "/arm-io/interrupt-controller", &node) != kSuccess ||
            SecureDTGetProperty(node, "reg", &property, &bytes) != kSuccess || bytes != 48) {
            panic("BEGONIA GICv3: missing device-tree register ranges (expected 3 ranges)");
        }
        const uint64_t *reg = property;
        vm_offset_t soc = pe_arm_get_soc_base_phys();
        gicd_base  = ml_io_map(soc + reg[0], reg[1]);
        gicr_base  = ml_io_map(soc + reg[2], reg[3]);
        gicc_base  = ml_io_map(soc + reg[4], reg[5]);
        if (!gicd_base || !gicr_base || !gicc_base) {
            panic("BEGONIA GICv3: mapping failed");
        }
        kprintf("BEGONIA GICv3: mapped DIST=0x%lx REDIST=0x%lx CPU=0x%lx\n",
                (unsigned long)gicd_base, (unsigned long)gicr_base, (unsigned long)gicc_base);
    }
    unsigned logical = cpu_number();
    uint32_t gicd_ctlr = read_reg(gicd_base, 0);
    unsigned gicd_typer = read_reg(gicd_base, 4);
    unsigned num_spis = ((gicd_typer >> 0) & 0x1f) + 1;
    unsigned num_ppi = (gicd_typer >> 5) & 0x7;
    unsigned num_sgi = (gicd_typer >> 8) & 0x7;
    kprintf("BEGONIA GICv3: GICD_CTLR=0x%x TYPER=0x%x SPIs=%u PPIs=%u SGIs=%u\n",
            gicd_ctlr, gicd_typer, num_spis * 32, num_ppi * 16, num_sgi * 16);
    for (unsigned group = 1; group < ((read_reg(gicd_base, 4) & 31) + 1); group++) {
        write_reg(gicd_base, 0x180 + group * 4, UINT32_MAX);
    }
    unsigned my_rd_base = gicr_base + (logical * 0x20000);
    write_reg(my_rd_base + 0x40, 0x1); /* GICR_WAKER: clear ProcessorSleep */
    while (read_reg(my_rd_base + 0x40) & 0x2) { }
    write_reg(my_rd_base + 0x00, 0x1); /* GICR_CTLR: enable redistributor */
    write_reg(gicd_base, 0x2000, 0x1); /* GICD_ISENABLER0: enable SGI 0..15 */
    write_reg(gicd_base, 0x2000, 0x1 << 27); /* Enable virtual timer PPI (INTID 27) */
    unsigned offsets[2] = { 27, BEGONIA_IPI_SGI };
    for (unsigned i = 0; i < 2; i++) {
        unsigned offset = 0x400 + (offsets[i] & ~3u);
        unsigned shift = (offsets[i] & 3u) * 8;
        uint32_t priority = read_reg(gicd_base, offset);
        write_reg(gicd_base, offset, (priority & ~(0xffu << shift)) | (0xa0u << shift));
    }
    write_reg(my_rd_base + 0x100, 1u << 27); /* Enable PPI 27 in redistributor */
    write_reg(my_rd_base + 0x100, 1u << BEGONIA_IPI_SGI); /* Enable SGI in redistributor */
    write_reg(gicc_base, 0x0000, 0x00); /* ICC_PMR_EL1: accept all priorities */
    write_reg(gicc_base, 0x0004, 0x02); /* ICC_BPR1_EL1: group priority split */
    write_reg(gicc_base, 0x0010, 0x01); /* ICC_CTLR_EL1: enable Group 1, EOI mode */
    write_reg(gicd_base, 0x0000, read_reg(gicd_base, 0x0000) | 0x03); /* GICD_CTLR: EnableGrp0 | EnableGrp1 */
    __builtin_arm_dsb(DSB_SY);
    __builtin_arm_isb(ISB_SY);
    kprintf("BEGONIA GICv3: boot CPU init complete, timer INTID %u, IPI SGI %u\n",
            BEGONIA_VTIMER_INTID, BEGONIA_IPI_SGI);
}
uint32_t begonia_gic_acknowledge(void)
{
    uint32_t iar = read_reg(gicc_base, 0x000c);
    if ((iar & 0x3ff) == BEGONIA_IPI_SGI) {
        __atomic_add_fetch(&ipi_count, 1, __ATOMIC_RELAXED);
    }
    if ((iar & 0x3ff) == BEGONIA_VTIMER_INTID) {
        __atomic_add_fetch(&timer_count, 1, __ATOMIC_RELAXED);
    }
    return iar;
}
void begonia_gic_complete(uint32_t iar)
{
    __builtin_arm_dsb(DSB_ISH);
    write_reg(gicc_base, 0x0010, iar); /* ICC_EOIR1_EL1 */
    __builtin_arm_isb(ISB_SY);
}
uint32_t begonia_gic_ipi_count(void) { return ipi_count; }
uint32_t begonia_gic_timer_count(void) { return timer_count; }
void begonia_gic_signal_cpu(unsigned logical)
{
    if (logical >= MAX_CPUS) panic("BEGONIA GICv3: bad IPI CPU");
    uint32_t target = __atomic_load_n(&cpu_targets[logical], __ATOMIC_ACQUIRE);
    if (!target) panic("BEGONIA GICv3: CPU target not initialized");
    __builtin_arm_dsb(DSB_ISHST);
    write_reg(gicd_base, 0xf00, (target << 16) | BEGONIA_IPI_SGI);
    __builtin_arm_isb(ISB_SY);
}
void begonia_gic_signal_self(void)
{
    __builtin_arm_dsb(DSB_ISHST);
    write_reg(gicd_base, 0xf00, (2u << 24) | BEGONIA_IPI_SGI);
    __builtin_arm_isb(ISB_SY);
}
void begonia_gic_secondary_init(unsigned logical)
{
    if (logical >= MAX_CPUS) panic("BEGONIA GICv3: bad secondary CPU");
    unsigned my_rd_base = gicr_base + (logical * 0x20000);
    write_reg(my_rd_base + 0x40, 0x1);
    while (read_reg(my_rd_base + 0x40) & 0x2) { }
    write_reg(my_rd_base + 0x00, 0x1);
    write_reg(my_rd_base + 0x100, 1u << 27);
    write_reg(my_rd_base + 0x100, 1u << BEGONIA_IPI_SGI);
    uint32_t target = read_reg(gicd_base, 0x800 + (logical * 8)) & 0xff;
    if (!target || (target & (target - 1))) {
        panic("BEGONIA GICv3: invalid CPU target mask for CPU %u", logical);
    }
    __atomic_store_n(&cpu_targets[logical], target, __ATOMIC_RELEASE);
    write_reg(gicc_base, 0x0000, 0x00);
    write_reg(gicc_base, 0x0004, 0x02);
    write_reg(gicc_base, 0x0010, 0x01);
    __builtin_arm_dsb(DSB_SY);
    __builtin_arm_isb(ISB_SY);
}
#endif