/*
 * Built-in IOWatchdog for A04BR3.
 *
 * watchdogd (launchd job with _PanicOnCrash) opens IOServiceMatching(
 * "IOWatchdog") with type 1 and exits when it is missing, which panics the
 * system. On Apple hardware the kext arms a SoC watchdog that resets the
 * device unless watchdogd keeps checking in. A04BR3 has no reset path, so
 * this full-root-only development service accepts known calls (check-in 1, enable/disable userspace
 * monitoring 3/4, 0/5, struct calls 2 and 9) and does nothing; each
 * selector is logged the first time it is used. Unknown selectors fail.
 * IOPMUPrimary acknowledges development boot-stage writes; there is no PMU
 * or hardware watchdog implementation. Based on ios-sweet 0040 and 0045
 * at fdad0de8dc0e75c5ea84762adc03f7232dc91f19.
 */
#include <pexpert/arm64/board_config.h>

#if defined(A04BR3)
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <sys/proc.h>
#include <pexpert/pexpert.h>

class IOWatchdog : public IOService {
	OSDeclareDefaultStructors(IOWatchdog);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IOWatchdog, IOService);

IOReturn
IOWatchdog::setProperties(OSObject *properties)
{
	/* Development boot-stage acknowledgement only; no Apple PMU exists. */
	return OSDynamicCast(OSDictionary, properties) ? kIOReturnSuccess : kIOReturnBadArgument;
}

bool
IOWatchdog::start(IOService *provider)
{
	unsigned enabled = 0;
	if (!PE_parse_boot_argn("a04br3.full_root", &enabled, sizeof(enabled)) || enabled != 1) return false;
	if (!IOService::start(provider)) {
		return false;
	}
	setProperty(kIOUserClientClassKey, "IOWatchdogUserClient");
	setProperty("IOPMUPrimary", 1ULL, 32);
	setProperty("A04BR3DevelopmentOnly", kOSBooleanTrue);
	registerService();
	IOLog("A04BR3 WDT: IOWatchdog registered (accepts check-ins, never fires)\n");
	return true;
}

class IOWatchdogUserClient : public IOUserClient {
	OSDeclareDefaultStructors(IOWatchdogUserClient);

public:
	IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target,
	    void *reference) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IOWatchdogUserClient, IOUserClient);

IOReturn
IOWatchdogUserClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

static uint64_t a04br3_wdt_logged;       /* selectors already logged (< 64) */

IOReturn
IOWatchdogUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    __unused IOExternalMethodDispatch *dispatch, __unused OSObject *target,
    __unused void *reference)
{
	/* Only the selectors used by the reference watchdogd contract. */
	if (selector > 5 && selector != 9) return kIOReturnUnsupported;
	if (args == NULL || args->scalarInputCount > 16 || args->scalarOutputCount > 16 ||
	    args->structureInputSize > 4096 || args->structureOutputSize > 4096 ||
	    (args->structureOutputDescriptor && args->structureOutputDescriptor->getLength() > 4096)) {
		return kIOReturnBadArgument;
	}

	for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
		args->scalarOutput[i] = 0;
	}
	if (args->structureOutputDescriptor != NULL) {
		IOByteCount len = args->structureOutputDescriptor->getLength();
		for (IOByteCount off = 0; off < len; off += 8) {
			static const uint64_t zero = 0;
			args->structureOutputDescriptor->writeBytes(off, &zero, len - off < 8 ? len - off : 8);
		}
	} else if (args->structureOutput != NULL) {
		bzero(args->structureOutput, args->structureOutputSize);
	}

	if (selector >= 64 || !(a04br3_wdt_logged & (1ULL << selector))) {
		if (selector < 64) {
			a04br3_wdt_logged |= 1ULL << selector;
		}
		char name[32];
		proc_selfname(name, sizeof(name));
		IOLog("A04BR3 WDT: %s sel %u in %u/%u out %u/%u -> ok\n", name, selector,
		    args->scalarInputCount, args->structureInputSize,
		    args->scalarOutputCount, args->structureOutputSize);
	}
	return kIOReturnSuccess;
}
#endif /* A04BR3 */
