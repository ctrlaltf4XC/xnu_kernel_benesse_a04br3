/*
 * Opt-in Cortex-A35 SMP using XNU IOCPU, PSCI and MT8167 GICv2.
 * The MT8167 GICv2 routes the architected timer and SGI1 via sleh_irq.
 * Startup sequencing follows ios-sweet 0065; suspend remains unsupported.
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
#include <kern/thread.h>
#include <pexpert/pexpert.h>
extern "C" void a04br3_secondary_entry(void);
extern "C" boolean_t a04br3_cpu_signal_is_enabled(unsigned int);
class A04BR3CPU;
static A04BR3CPU *registered_cpus[MAX_CPUS];
static unsigned registered_count;

static int32_t psci_cpu_on(uint64_t target, uint64_t entry, uint64_t context)
{
    register uint64_t x0 __asm__("x0") = 0xc4000003ULL;
    register uint64_t x1 __asm__("x1") = target;
    register uint64_t x2 __asm__("x2") = entry;
    register uint64_t x3 __asm__("x3") = context;
    __asm__ volatile("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : :
        "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13",
        "x14", "x15", "x16", "x17", "cc", "memory");
    return (int32_t)x0;
}
#include <mach/mach_time.h>
#include <pexpert/arm/protos.h>

class A04BR3CPU : public IOCPU {
	OSDeclareDefaultStructors(A04BR3CPU);
	const OSSymbol *cpuName = NULL;
    unsigned initialized = 0;

public:
    static void startAll(void *, wait_result_t);
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
	const ml_topology_cpu_t *cpu;
	OSNumber *logical = OSDynamicCast(OSNumber, provider->getProperty("logical-cpu-id"));
	OSData *reg = OSDynamicCast(OSData, provider->getProperty("reg"));

	if (logical == NULL || reg == NULL || reg->getLength() != sizeof(uint32_t) ||
	    logical->unsigned32BitValue() >= topology->num_cpus) {
		return false;
	}
    cpu = &topology->cpus[logical->unsigned32BitValue()];
    if (topology->num_cpus > 4 || cpu->phys_id != cpu->cpu_id ||
        *(const uint32_t *)reg->getBytesNoCopy() != cpu->phys_id) {
        panic("A04BR3 CPU: invalid flat A35 topology");
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

    registerService();
    __atomic_store_n(&registered_cpus[cpu->cpu_id], this, __ATOMIC_RELEASE);
    __atomic_add_fetch(&registered_count, 1, __ATOMIC_ACQ_REL);
    if (cpu == topology->boot_cpu) {
        thread_t thread;
        result = kernel_thread_start(&A04BR3CPU::startAll, NULL, &thread);
        if (result != KERN_SUCCESS) panic("A04BR3 CPU: startup thread %d", result);
        thread_deallocate(thread);
    }
    return true;
}

void
A04BR3CPU::startAll(void *arg __unused, wait_result_t result __unused)
{
    const ml_topology_info_t *topology = ml_get_topology_info();
    uint64_t deadline;
    nanoseconds_to_deadline(10 * NSEC_PER_SEC, &deadline);
    while (__atomic_load_n(&registered_count, __ATOMIC_ACQUIRE) != topology->num_cpus) {
        if (mach_absolute_time() > deadline) panic("A04BR3 CPU: registration timeout");
        IOSleep(1);
    }
    if (topology->boot_cpu->cpu_id != 0 || topology->boot_cpu->phys_id != 0)
        panic("A04BR3 CPU: boot CPU must be zero");
    for (unsigned i = 0; i < topology->num_cpus; i++) {
        A04BR3CPU *cpu = __atomic_load_n(&registered_cpus[i], __ATOMIC_ACQUIRE);
        kern_return_t kr = processor_start(cpu->machProcessor);
        if (kr != KERN_SUCCESS) panic("A04BR3 CPU: start %u returned %d", i, kr);
        nanoseconds_to_deadline(5 * NSEC_PER_SEC, &deadline);
        while (!__atomic_load_n(&cpu->initialized, __ATOMIC_ACQUIRE) ||
            !a04br3_cpu_signal_is_enabled(i)) {
            if (mach_absolute_time() > deadline)
                panic("A04BR3 CPU: CPU %u reset/MMU/GIC startup timeout", i);
            IOSleep(1);
        }
        IOLog("A04BR3 CPU: CPU %u online, SGI1 verified\n", i);
    }
    /* MSI/storage remain serialized by their existing controller locks. */
    a04br3_watchdog_quiesce();
    ml_cpu_init_completed();
    __atomic_load_n(&registered_cpus[0], __ATOMIC_ACQUIRE)->cpusRunning();
    IOService::publishResource(gIOAllCPUInitializedKey, kOSBooleanTrue);
    ml_set_max_cpus(topology->max_cpu_id + 1);
    IOLog("A04BR3 CPU: %u/%u CPUs initialized\n", topology->num_cpus, topology->num_cpus);
    thread_terminate(current_thread());
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
    ml_broadcast_cpu_event(CPU_BOOTED, getCPUNumber());
    /* Secondary init runs with IRQs masked: wait for delivery in startAll. */
    signalCPU(this);
    setCPUState(kIOCPUStateRunning);
    __atomic_store_n(&initialized, 1, __ATOMIC_RELEASE);
}

void
A04BR3CPU::signalCPU(IOCPU *target)
{
    unsigned logical = target->getCPUNumber();
    if (logical >= ml_get_topology_info()->num_cpus)
        panic("A04BR3 CPU: invalid IPI target %u", logical);
    a04br3_gic_signal_cpu(logical);
}

kern_return_t
A04BR3CPU::startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr)
{
    unsigned logical = getCPUNumber();
    if (start_paddr || arg_paddr || logical == 0 || initialized)
        panic("A04BR3 CPU: unsupported restart %u", logical);
    vm_offset_t entry = ml_static_vtop((vm_offset_t)&a04br3_secondary_entry);
    __builtin_arm_dsb(DSB_SY);
    int32_t rc = psci_cpu_on(ml_get_topology_info()->cpus[logical].phys_id, entry, logical);
    IOLog("A04BR3 CPU: CPU_ON %u entry=0x%llx result=%d\n", logical, (uint64_t)entry, rc);
    if (rc) panic("A04BR3 CPU: CPU_ON %u failed %d", logical, rc);
    return KERN_SUCCESS;
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
