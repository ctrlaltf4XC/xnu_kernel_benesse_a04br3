/*
 * Single-CPU bring-up for a04br3 using XNU's legacy IOCPU interface.
 * The MT8167 GICv2 routes the architected timer and SGI1 via sleh_irq.
 * Secondary CPU startup and system suspend require separate platform work.
 */
#include <pexpert/arm64/board_config.h>

#if defined(A04BR3)
#include <IOKit/IOCPU.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSNumber.h>
#include <arm64/proc_reg.h>
#include <machine/atomic.h>
#include <kern/clock.h>
#include <kern/processor.h>
#include <mach/mach_time.h>
#include <pexpert/arm/protos.h>

class A04BR3CPU : public IOCPU {
	OSDeclareDefaultStructors(A04BR3CPU);
	const OSSymbol *cpuName = NULL;

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
	void initCPU(bool boot) APPLE_KEXT_OVERRIDE;
	void quiesceCPU(void) APPLE_KEXT_OVERRIDE;
	kern_return_t startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr) APPLE_KEXT_OVERRIDE;
	void haltCPU(void) APPLE_KEXT_OVERRIDE;
	void signalCPU(IOCPU *target) APPLE_KEXT_OVERRIDE;
	const OSSymbol *getCPUName(void) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(A04BR3CPU, IOCPU);

bool
A04BR3CPU::start(IOService *provider)
{
	const ml_topology_info_t *topology = ml_get_topology_info();
	const ml_topology_cpu_t *cpu = topology->boot_cpu;
	OSNumber *logical = OSDynamicCast(OSNumber, provider->getProperty("logical-cpu-id"));
	OSData *reg = OSDynamicCast(OSData, provider->getProperty("reg"));

	if (logical == NULL || reg == NULL || reg->getLength() != sizeof(uint32_t) ||
	    logical->unsigned32BitValue() != cpu->cpu_id ||
	    *(const uint32_t *)reg->getBytesNoCopy() != cpu->phys_id) {
		return false;
	}
	if (topology->num_cpus != 1) {
		panic("A04BR3 CPU: only single-CPU bring-up is supported");
	}
	IOLog("A04BR3 CPU: starting boot CPU registration\n");
	if (!IOCPU::start(provider)) {
		return false;
	}
	cpuName = OSSymbol::withCStringNoCopy("Cortex-A35");
	if (cpuName == NULL) {
		return false;
	}
	setCPUNumber(cpu->cpu_id);

	ml_processor_info_t info = {};
	info.cpu_id = reinterpret_cast<cpu_id_t>(this);
	info.phys_id = cpu->phys_id;
	info.log_id = cpu->cpu_id;
	info.cluster_id = cpu->cluster_id;
	info.cluster_type = cpu->cluster_type;
	info.l2_cache_id = cpu->l2_cache_id;
	info.l2_cache_size = cpu->l2_cache_size;
	info.l3_cache_id = cpu->l3_cache_id;
	info.l3_cache_size = cpu->l3_cache_size;
	perfmon_interrupt_handler_func pmi_handler = NULL;
	kern_return_t result = ml_processor_register(&info, &machProcessor, &ipi_handler, &pmi_handler);
	if (result != KERN_SUCCESS) {
		panic("A04BR3 CPU: ml_processor_register failed: %d", result);
	}
	if (pmi_handler != NULL) {
		panic("A04BR3 CPU: PMI routing is not implemented");
	}

	/* Re-enter the boot CPU through cpu_machine_init -> PE_cpu_machine_init. */
	IOLog("A04BR3 CPU: processor registered; starting boot CPU\n");
	result = processor_start(machProcessor);
	if (result != KERN_SUCCESS || getCPUState() != kIOCPUStateRunning) {
		panic("A04BR3 CPU: processor_start failed: %d", result);
	}
	/* Late platform stage, after IRQ self-test: never touch TOPRGU in shim. */
	a04br3_watchdog_quiesce();
	IOLog("A04BR3 CPU: timer IRQ count %u, self IPI count %u\n",
	    a04br3_gic_timer_count(), a04br3_gic_ipi_count());
	ml_cpu_init_completed();
	cpusRunning();
	registerService();
	IOService::publishResource(gIOAllCPUInitializedKey, kOSBooleanTrue);

	/* Announce completion only after registration, startup and SGI validation. */
	ml_set_max_cpus(topology->max_cpu_id + 1);
	IOLog("A04BR3 CPU: boot CPU %u registered, SGI1 verified, initialization complete\n",
	    getCPUNumber());
	return true;
}

void
A04BR3CPU::free(void)
{
	OSSafeReleaseNULL(cpuName);
	IOCPU::free();
}

void
A04BR3CPU::initCPU(bool boot)
{
	if (!boot) {
		panic("A04BR3 CPU: resume is not implemented");
	}
	assert(cpu_number() == (int)getCPUNumber());
	assert(ml_get_interrupts_enabled());
	ml_broadcast_cpu_event(CPU_BOOTED, getCPUNumber());

	IOLog("A04BR3 CPU: validating GICv2 self SGI\n");
	/* A real GIC interrupt clears SIGPdisabled in cpu_signal_handler. */
	uint64_t deadline;
	nanoseconds_to_deadline(NSEC_PER_SEC, &deadline);
	uint32_t previous_ipis = a04br3_gic_ipi_count();
	signalCPU(this);
	while (a04br3_gic_ipi_count() == previous_ipis || !ml_cpu_signal_is_enabled()) {
		if (mach_absolute_time() > deadline) {
			panic("A04BR3 CPU: SGI1 self-test timed out");
		}
		__asm__ volatile("yield");
	}
	setCPUState(kIOCPUStateRunning);
}

void
A04BR3CPU::signalCPU(IOCPU *target)
{
	/* Only the boot CPU is registered by this driver. */
	if (target != this || cpu_number() != (int)getCPUNumber()) {
		panic("A04BR3 CPU: secondary CPU signalling is not implemented");
	}
	a04br3_gic_signal_self();
}

kern_return_t
A04BR3CPU::startCPU(vm_offset_t start_paddr __unused, vm_offset_t arg_paddr __unused)
{
	return KERN_NOT_SUPPORTED;
}

void
A04BR3CPU::quiesceCPU(void)
{
	panic("A04BR3 CPU: CPU quiesce is not implemented");
}

void
A04BR3CPU::haltCPU(void)
{
	panic("A04BR3 CPU: CPU halt/suspend is not implemented");
}

const OSSymbol *
A04BR3CPU::getCPUName(void)
{
	return cpuName;
}
#endif /* A04BR3 */
