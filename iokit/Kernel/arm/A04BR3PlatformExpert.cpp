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

class A04BR3PlatformExpert : public IODTPlatformExpert {
	OSDeclareDefaultStructors(A04BR3PlatformExpert);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
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
	registerService();
	IOLog("A04BR3: platform expert registered\n");
	return true;
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
	strlcpy(name, "Benesse TAB-A04-BR3", (size_t)maxLength);
	return true;
}
#endif /* A04BR3 */
