/* SPDX-License-Identifier: MIT
 * MT8167 non-secure GICv2 boot-CPU timer/SGI bring-up.
 * Register semantics: Arm GICv2 and existing A04BR3 EDK2 GICv2 implementation.
 * Secure Group 0 ownership is left to ATF. No GICv3 system registers are used.
 */
#include <pexpert/arm64/board_config.h>
#if defined(A04BR3)
#include <stdint.h>
#include <kern/misc_protos.h>
#include <machine/machine_routines.h>
#include <arm64/proc_reg.h>
#include <pexpert/device_tree.h>
#include <pexpert/arm/protos.h>
static vm_offset_t distributor, cpu_interface;
static uint32_t read_reg(vm_offset_t base, unsigned offset)
{
    return *(volatile uint32_t *)(base + offset);
}
static void write_reg(vm_offset_t base, unsigned offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}
void a04br3_gic_init(void)
{
    if (!distributor) {
        DTEntry node;
        const void *property;
        unsigned int bytes;
        if (SecureDTLookupEntry(NULL, "/arm-io/interrupt-controller", &node) != kSuccess ||
            SecureDTGetProperty(node, "reg", &property, &bytes) != kSuccess || bytes != 32) {
            panic("A04BR3 GICv2: missing device-tree register ranges");
        }
        const uint64_t *reg = property;
        vm_offset_t soc = pe_arm_get_soc_base_phys();
        distributor = ml_io_map(soc + reg[0], reg[1]);
        cpu_interface = ml_io_map(soc + reg[2], reg[3]);
        if (!distributor || !cpu_interface) { panic("A04BR3 GICv2: mapping failed"); }
        /* Disable inherited non-secure SPIs until their device drivers exist.
         * Non-secure accesses cannot change Secure Group 0 enable bits. */
        unsigned groups = (read_reg(distributor, 4) & 31) + 1;
        for (unsigned group = 1; group < groups; group++) {
            write_reg(distributor, 0x180 + group * 4, UINT32_MAX);
        }
        kprintf("A04BR3 GICv2: mapped, timer INTID %u, self SGI %u\n",
            A04BR3_VTIMER_INTID, A04BR3_IPI_SGI);
    }
    /* Banked SGI/PPI state for the boot CPU. Keep secure routing unchanged. */
    write_reg(distributor, 0x180, UINT32_MAX);
    write_reg(distributor, 0x280, 1u << A04BR3_VTIMER_INTID);
    unsigned offsets[2] = { A04BR3_VTIMER_INTID, A04BR3_IPI_SGI };
    for (unsigned i = 0; i < 2; i++) {
        unsigned offset = 0x400 + (offsets[i] & ~3u);
        unsigned shift = (offsets[i] & 3u) * 8;
        uint32_t priority = read_reg(distributor, offset);
        write_reg(distributor, offset, (priority & ~(0xffu << shift)) | (0xa0u << shift));
    }
    write_reg(distributor, 0x100, (1u << A04BR3_VTIMER_INTID) | (1u << A04BR3_IPI_SGI));
    write_reg(cpu_interface, 4, 0xff); /* GICC_PMR */
    write_reg(cpu_interface, 8, 0);    /* GICC_BPR */
    /* Non-secure CTLR bit 0 enables Group 1. Disable split EOI mode (bit 9). */
    write_reg(cpu_interface, 0, (read_reg(cpu_interface, 0) & ~(1u << 9)) | 1u);
    write_reg(distributor, 0, read_reg(distributor, 0) | 1u);
    __builtin_arm_dsb(DSB_SY);
    __builtin_arm_isb(ISB_SY);
}
uint32_t a04br3_gic_acknowledge(void)
{
    return read_reg(cpu_interface, 0x0c);
}
void a04br3_gic_complete(uint32_t iar)
{
    __builtin_arm_dsb(DSB_ISH);
    write_reg(cpu_interface, 0x10, iar); /* Preserve IAR source CPU for SGIs. */
    __builtin_arm_isb(ISB_SY);
}
void a04br3_gic_signal_self(void)
{
    __builtin_arm_dsb(DSB_ISHST);
    write_reg(distributor, 0xf00, (2u << 24) | A04BR3_IPI_SGI);
    __builtin_arm_isb(ISB_SY);
}
#endif
