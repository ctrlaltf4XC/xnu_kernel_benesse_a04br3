/*
 * Minimal device-tree platform expert for Benesse TAB-A04-BR3 (a04br3).
 * Uses XNU's normal platform, power-domain and nub initialization. Hardware
 * interrupt and power-management services are supplied by separate drivers.
 */
#include <pexpert/arm64/board_config.h>

#if defined(A04BR3)
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include "A04BR3Restart.h"
extern "C" vm_offset_t ml_io_map(vm_offset_t,vm_size_t);

class A04BR3PlatformExpert : public IODTPlatformExpert {
	OSDeclareDefaultStructors(A04BR3PlatformExpert);
	vm_offset_t restartBase=0;
	static uint32_t restartRead(void *p,unsigned offset) {
		return *(volatile uint32_t *)((uintptr_t)p+offset);
	}
	static void restartWrite(void *p,unsigned offset,uint32_t value) {
		*(volatile uint32_t *)((uintptr_t)p+offset)=value;
	}
	static void restartBarrier(void *) {__asm__ volatile("dsb sy":::"memory");}
	static void restartDelay(void *,unsigned us) {IODelay(us);}

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	int haltRestart(unsigned int type) APPLE_KEXT_OVERRIDE;
	void processTopLevel(IORegistryEntry *root) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	bool getTargetName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	bool getProductName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(A04BR3PlatformExpert, IODTPlatformExpert);

bool
A04BR3PlatformExpert::start(IOService *provider)
{
	IOLog("A04BR3: starting device-tree platform expert\n");
	if (!IODTPlatformExpert::start(provider)) {
		return false;
	}
	/* Permanent device mapping now, never allocate during shutdown/panic. */
	restartBase=ml_io_map(0x10007000,0x1000);
	IOLog("A04BR3: TOPRGU normal restart mapping=%u\n",restartBase?1U:0U);
	registerService();
	IOLog("A04BR3: platform expert registered\n");
	return true;
}

int
A04BR3PlatformExpert::haltRestart(unsigned int type)
{
	/* Panic sync/begin/end, halt, hang and panic restart are NOT ordinary
	 * reboot requests. Preserve superclass semantics; never turn halt into
	 * EDK's ResetShutdown fallback-to-reset. No PMIC power-off claimed. */
	if(type!=kPERestartCPU)return IODTPlatformExpert::haltRestart(type);
	if(!restartBase)return -1;
	IOLog("A04BR3: normal restart via TOPRGU WDT (no recovery flag)\n");
	a04_restart_io io={(void *)restartBase,restartRead,restartWrite,restartBarrier,restartDelay};
	if(!a04_restart_request(&io))return -1;
	/* SWRST normally never returns. No repeated reset writes or success
	 * return if the board did not reset; halt_all_cpus remains terminal. */
	IODelay(1000);
	IOLog("A04BR3: TOPRGU reset request returned; reset unconfirmed\n");
	return -1;
}

const char *
A04BR3PlatformExpert::deleteList(void)
{
	return "()";
}

const char *
A04BR3PlatformExpert::excludeList(void)
{
	return "('chosen', 'defaults', 'cpus', 'options')";
}

void
A04BR3PlatformExpert::processTopLevel(IORegistryEntry *root)
{
	/*
	 * The stock implementation requires an Apple NVRAM /options node.
	 * A04BR3 has no NVRAM controller; publish real CPU/device nodes without
	 * inventing one. createNubs adds the topology's logical CPU/cluster IDs.
	 */
	IORegistryEntry *cpus = root->childFromPath("cpus", gIODTPlane);
	if (cpus != NULL) {
		createNubs(this, IODTFindMatchingEntries(cpus, kIODTExclusive, NULL));
		cpus->release();
	}
	createNubs(this, IODTFindMatchingEntries(root, kIODTExclusive, excludeList()));
}

bool
A04BR3PlatformExpert::getTargetName(char *name, int maxLength)
{
	if (name == NULL || maxLength <= 0) {
		return false;
	}
	strlcpy(name, "a04br3", (size_t)maxLength);
	return true;
}

bool
A04BR3PlatformExpert::getProductName(char *name, int maxLength)
{
	if (name == NULL || maxLength <= 0) {
		return false;
	}
	/*
	 * MobileGestalt derives DeviceClass from the hw.product prefix. An
	 * unrecognized prefix falls back to iPod; this board is a tablet.
	 * Keep a distinct port product, not an Apple model identifier. The ADT
	 * model (and inherited hw.machine) remains Benesse TAB-A04-BR3, while
	 * hw.target remains a04br3 for board-specific driver selection.
	 */
	strlcpy(name, "iPad-A04BR3", (size_t)maxLength);
	return true;
}
#endif /* A04BR3 */
