extern "C" void a04br3_keys_init(void);
extern "C" void a04br3_touch_init(void);
/*
 * A04BR3: HID event services for the iOS userland (backboardd's HID event
 * system).
 *
 * backboardd watches for IOService objects with HIDServiceSupport = true,
 * opens them with IOServiceOpen type 'esuc' (IOHIDEventServiceUserClient,
 * compiled here from IOHIDFamily-1915.140.3, the version the iOS 16.7
 * userland ships) and reads serialized IOHIDEvents from the user client's
 * shared queue. This file has the minimal IOHIDEventService base class and
 * two services:
 *
 *   A04BR3HIDButtons  consumer-control keys (power, volume, home)
 *   A04BR3HIDTouch    the built-in touch screen (digitizer collections)
 *
 * Hardware drivers feed them through the C entry points a04br3_hid_key()
 * and a04br3_hid_touch(). Boot-arg a04br3hidtest=N (seconds) presses Home
 * every N seconds, to test the event path without input hardware.
 *
 * Private type0xa04b is explicitly opened on A04BR3HIDButtons by root and
 * requires a04br3.console=1. It dispatches finite Home/tap pairs only; native
 * esuc/default user clients, settings and HID identity stay unchanged.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <pexpert/pexpert.h>
#include <libkern/OSAtomic.h>
#include <kern/task.h>
#include "A04BR3UITest.h"

#include "IOHIDEventService.h"
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEvent.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDUsageTables.h"
#include "A04BR3HIDSettings.h"

extern "C" uint64_t mach_absolute_time(void);

static IOReturn a04_ui_open(IOHIDEventService *, task_t, void *, UInt32,
    OSDictionary *, IOUserClient **);

/* One open client: the user client and its event callback. */
class IOHIDEventServiceClient : public OSObject {
	OSDeclareDefaultStructors(IOHIDEventServiceClient);
public:
	bool                       initClient() { return OSObject::init(); }
	IOService                 *client;
	void                      *context;
	IOHIDEventService::Action  action;
};
OSDefineMetaClassAndStructors(IOHIDEventServiceClient, OSObject);

#define super IOService
OSDefineMetaClassAndStructors(IOHIDEventService, IOService);

bool
IOHIDEventService::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}
	_clientLock = IOLockAlloc();
	_clients = OSArray::withCapacity(2);
	_hidWorkLoop = getWorkLoop();
	if (!_clientLock || !_clients || !_hidWorkLoop) {
		return false;
	}
	_hidWorkLoop->retain();
	_hidGate = IOCommandGate::commandGate(this);
	if (!_hidGate || _hidWorkLoop->addEventSource(_hidGate) != kIOReturnSuccess) {
		return false;
	}

	OSString *transport = getTransport();
	OSString *product = getProduct();
	setProperty(kIOHIDTransportKey, transport);
	setProperty(kIOHIDProductKey, product);
	setProperty(kIOHIDManufacturerKey, "A04BR3");
	setProperty(kIOHIDVendorIDKey, getVendorID(), 32);
	setProperty(kIOHIDProductIDKey, getProductID(), 32);
	setProperty(kIOHIDLocationIDKey, (unsigned long long)getProductID(), 32);
	setProperty(kIOHIDReportIntervalKey, getReportInterval(), 32);
	setProperty(kIOHIDPrimaryUsagePageKey, getPrimaryUsagePage(), 32);
	setProperty(kIOHIDPrimaryUsageKey, getPrimaryUsage(), 32);
	setProperty(kIOHIDBuiltInKey, kOSBooleanTrue);
	OSSafeReleaseNULL(transport);
	OSSafeReleaseNULL(product);

	OSDictionary *pair = OSDictionary::withCapacity(2);
	OSArray *pairs = OSArray::withCapacity(1);
	OSNumber *page = OSNumber::withNumber(getPrimaryUsagePage(), 32);
	OSNumber *usage = OSNumber::withNumber(getPrimaryUsage(), 32);
	if (pair && pairs && page && usage) {
		pair->setObject(kIOHIDDeviceUsagePageKey, page);
		pair->setObject(kIOHIDDeviceUsageKey, usage);
		pairs->setObject(pair);
		setProperty(kIOHIDDeviceUsagePairsKey, pairs);
	}
	OSSafeReleaseNULL(pair);
	OSSafeReleaseNULL(pairs);
	OSSafeReleaseNULL(page);
	OSSafeReleaseNULL(usage);

	/* What IOHIDFamily's "IOHIDEventServiceUserClient" personality merges in. */
	setProperty("HIDServiceSupport", kOSBooleanTrue);
	setProperty("IOUserClientClass", "IOHIDEventServiceUserClient");
	return true;
}

void
IOHIDEventService::free(void)
{
	if (_hidGate && _hidWorkLoop) {
		_hidWorkLoop->removeEventSource(_hidGate);
	}
	OSSafeReleaseNULL(_hidGate);
	OSSafeReleaseNULL(_hidWorkLoop);
	OSSafeReleaseNULL(_clients);
	if (_clientLock) {
		IOLockFree(_clientLock);
		_clientLock = NULL;
	}
	super::free();
}

bool
IOHIDEventService::open(IOService *client, IOOptionBits options, void *context, Action action)
{
	IOHIDEventServiceClient *data = OSTypeAlloc(IOHIDEventServiceClient);
	bool ok = false;

	if (data && data->initClient()) {
		data->client = client;
		data->context = context;
		data->action = action;
		ok = super::open(client, options, data);
	}
	OSSafeReleaseNULL(data);
	IOLog("A04BR3 HID: %s open by %s -> %d\n", getName(), client ? client->getName() : "?", ok);
	return ok;
}

void
IOHIDEventService::close(IOService *forClient, IOOptionBits options)
{
	super::close(forClient, options);
}

bool
IOHIDEventService::handleOpen(IOService *client, IOOptionBits options __unused, void *argument)
{
	IOHIDEventServiceClient *data = OSDynamicCast(IOHIDEventServiceClient, (OSObject *)argument);
	bool ok = false;

	if (!data) {
		return false;
	}
	IOLockLock(_clientLock);
	if (!handleIsOpenLocked(client)) {
		/* Copy on write: dispatchEvent iterates a snapshot without the lock. */
		OSArray *next = OSArray::withArray(_clients, _clients->getCount() + 1);
		if (next && next->setObject(data)) {
			_clients->release();
			_clients = next;
			ok = true;
		} else {
			OSSafeReleaseNULL(next);
		}
	}
	IOLockUnlock(_clientLock);
	return ok;
}

void
IOHIDEventService::handleClose(IOService *client, IOOptionBits options __unused)
{
	IOLockLock(_clientLock);
	OSArray *next = OSArray::withCapacity(_clients->getCount());
	if (next) {
		for (unsigned int i = 0; i < _clients->getCount(); i++) {
			IOHIDEventServiceClient *data = (IOHIDEventServiceClient *)_clients->getObject(i);
			if (data->client != client) {
				next->setObject(data);
			}
		}
		_clients->release();
		_clients = next;
	}
	IOLockUnlock(_clientLock);
}

bool
IOHIDEventService::handleIsOpenLocked(const IOService *client) const
{
	for (unsigned int i = 0; i < _clients->getCount(); i++) {
		IOHIDEventServiceClient *data = (IOHIDEventServiceClient *)_clients->getObject(i);
		if (client == NULL || data->client == client) {
			return true;
		}
	}
	return false;
}

bool
IOHIDEventService::handleIsOpen(const IOService *client) const
{
	IOLockLock(_clientLock);
	bool open = handleIsOpenLocked(client);
	IOLockUnlock(_clientLock);
	return open;
}

void
IOHIDEventService::dispatchEvent(IOHIDEvent *event, IOOptionBits options)
{
	OSArray *clients;

	event->setSenderID(getRegistryEntryID());
	IOLockLock(_clientLock);
	clients = _clients;
	clients->retain();
	IOLockUnlock(_clientLock);

	for (unsigned int i = 0; i < clients->getCount(); i++) {
		IOHIDEventServiceClient *data = (IOHIDEventServiceClient *)clients->getObject(i);
		if (data->action) {
			(*data->action)(data->client, this, data->context, event, options);
		}
	}
	clients->release();
}

IOHIDEvent *
IOHIDEventService::copyEvent(IOHIDEventType type __unused, IOHIDEvent *matching __unused, IOOptionBits options __unused)
{
	return NULL;
}

IOHIDEvent *
IOHIDEventService::copyMatchingEvent(OSDictionary *matching __unused)
{
	return NULL;
}

IOReturn
IOHIDEventService::setElementValue(UInt32 usagePage __unused, UInt32 usage __unused, UInt32 value __unused)
{
	return kIOReturnUnsupported;
}

IOReturn
IOHIDEventService::setProperties(OSObject *properties)
{
	OSDictionary *updates = OSDynamicCast(OSDictionary, properties);
	if (!updates) return kIOReturnBadArgument;
	if (!_clientLock) return kIOReturnNotReady;
	/* Preserve actual event-system parameters, as the native base service does.
	 * This nested cache is separate from primary usage/transport identity. */
	IOLockLock(_clientLock);
	OSObject *stored = copyProperty(a04_hid_settings_key);
	OSDictionary *next = a04_hid_settings_copy(OSDynamicCast(OSDictionary, stored), updates);
	IOReturn result = kIOReturnNoMemory;
	if (next && setProperty(a04_hid_settings_key, next)) result = kIOReturnSuccess;
	OSSafeReleaseNULL(next);
	OSSafeReleaseNULL(stored);
	IOLockUnlock(_clientLock);
	return result;
}

UInt32
IOHIDEventService::getPrimaryUsagePage()
{
	return 0;
}

UInt32
IOHIDEventService::getPrimaryUsage()
{
	return 0;
}

OSString *
IOHIDEventService::getTransport()
{
	return OSString::withCString("SPU");
}

OSString *
IOHIDEventService::getProduct()
{
	return OSString::withCString(getName());
}

UInt32
IOHIDEventService::getVendorID()
{
	return 0x05ac;
}

UInt32
IOHIDEventService::getProductID()
{
	return 0;
}

UInt32
IOHIDEventService::getReportInterval()
{
	return 10000;	/* us */
}

IOReturn
IOHIDEventService::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    OSDictionary *properties, IOUserClient **handler)
{
	IOUserClient *client;

	if (type == A04_UI_CLIENT_TYPE) {
		return a04_ui_open(this, owningTask, securityID, type, properties, handler);
	}
	if (type != kIOHIDEventServiceUserClientType) {
		IOLog("A04BR3 HID: %s user client type 0x%x unsupported\n", getName(), (unsigned)type);
		return super::newUserClient(owningTask, securityID, type, properties, handler);
	}
	client = OSTypeAlloc(IOHIDEventServiceUserClient);
	if (!client) {
		return kIOReturnNoMemory;
	}
	if (!client->initWithTask(owningTask, securityID, type)) {
		client->release();
		IOLog("A04BR3 HID: %s user client init refused\n", getName());
		return kIOReturnBadArgument;
	}
	if (!client->attach(this)) {
		client->release();
		return kIOReturnUnsupported;
	}
	if (!client->start(this)) {
		client->detach(this);
		client->release();
		return kIOReturnUnsupported;
	}
	*handler = client;
	return kIOReturnSuccess;
}

/*
 * Buttons. iOS takes the side/volume/home buttons as consumer-control
 * keyboard events (AppleM68Buttons publishes the same usage pair).
 */
class A04BR3HIDButtons : public IOHIDEventService {
	OSDeclareDefaultStructors(A04BR3HIDButtons);
public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void key(UInt32 usage, bool down);
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    IOReturn uiTest(const uint64_t *request) { return _hidGate ? _hidGate->attemptAction(uiGated, (void *)request) : kIOReturnNotReady; }
private:
    static IOReturn uiGated(OSObject *, void *, void *, void *, void *);
    static IOReturn keyGated(OSObject *, void *, void *, void *, void *);
    static IOReturn unpublish(OSObject *, void *, void *, void *, void *);
    UInt32 fHeldKeys;
    bool fUIActive;
protected:
	virtual UInt32 getPrimaryUsagePage() APPLE_KEXT_OVERRIDE { return kHIDPage_Consumer; }
	virtual UInt32 getPrimaryUsage() APPLE_KEXT_OVERRIDE { return kHIDUsage_Csmr_ConsumerControl; }
	virtual UInt32 getProductID() APPLE_KEXT_OVERRIDE { return 1; }
private:
	static void testFired(OSObject *owner, IOTimerEventSource *sender);
	IOTimerEventSource *fTest;
	UInt32 fTestSecs;
	UInt32 fTestCount;
};
OSDefineMetaClassAndStructors(A04BR3HIDButtons, IOHIDEventService);

/*
 * Touch screen: digitizer collection events with finger children,
 * coordinates normalized to 0..1 like IOHIDEventDriver produces for
 * absolute digitizers.
 */
#define A04BR3_TOUCH_FINGERS 1
class A04BR3HIDTouch : public IOHIDEventService {
	OSDeclareDefaultStructors(A04BR3HIDTouch);
public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void touch(int finger, bool down, int x, int y);
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    IOWorkLoop *uiWorkLoop() const { return _hidWorkLoop; }
    bool uiBusy() const { return fHardwareDown || fDown[0]; }
    void uiDispatch(IOHIDEvent *event) { dispatchEvent(event); }
private:
    static IOReturn touchGated(OSObject *, void *, void *, void *, void *);
    static IOReturn unpublish(OSObject *, void *, void *, void *, void *);
    bool fHardwareDown;
protected:
	virtual UInt32 getPrimaryUsagePage() APPLE_KEXT_OVERRIDE { return kHIDPage_Digitizer; }
	virtual UInt32 getPrimaryUsage() APPLE_KEXT_OVERRIDE { return kHIDUsage_Dig_TouchScreen; }
	virtual UInt32 getProductID() APPLE_KEXT_OVERRIDE { return 2; }
private:
	IOLock *fLock;
	bool fDown[A04BR3_TOUCH_FINGERS];
	bool fEdge[A04BR3_TOUCH_FINGERS];
	IOFixed fX[A04BR3_TOUCH_FINGERS];
	IOFixed fY[A04BR3_TOUCH_FINGERS];
};
OSDefineMetaClassAndStructors(A04BR3HIDTouch, IOHIDEventService);

static A04BR3HIDButtons *gA04BR3Buttons;
static A04BR3HIDTouch *gA04BR3Touch;

/* Registry lock is never held over a gate/callback. It protects snapshots
 * against stop/detach; all event/state operations use the provider workloop.
 * The two workloops MUST match before the private API dispatches anything. */
static IOLock *gA04UIRegistryLock;
static bool a04_ui_registry_init()
{
    if (!gA04UIRegistryLock) {
        IOLock *lock = IOLockAlloc();
        if (!lock) return false;
        if (!OSCompareAndSwapPtr(NULL, lock, &gA04UIRegistryLock)) IOLockFree(lock);
    }
    return true;
}
static bool a04_ui_console()
{
    int enabled = 0;
    return PE_parse_boot_argn("a04br3.console", &enabled, sizeof(enabled)) && enabled == 1;
}

/* UI TEST BEGIN: finite pair, private client and shared touch event builder.
 * No timer, sleep, duration parameter or persistent synthetic down state.
 * Allocate BOTH complete events (including child arrays) before first send.
 * Release is dispatched immediately after down even if clientClose/Died runs
 * reentrantly. Normal HID queue delivery remains best-effort: enqueue has no
 * acknowledgement here, and this is not a hard real-time scheduler bound.
 */
static IOHIDEvent *a04_ui_touch_event(bool down, bool was, bool moved,
    bool edge, IOFixed x, IOFixed y)
{
    uint64_t now = mach_absolute_time();
    IOHIDEvent *collection = IOHIDEvent::digitizerEvent(now, 0,
        kIOHIDDigitizerTransducerTypeHand, false, 0, 0, 0);
    if (!collection) return NULL;
    IOHIDEvent *child = IOHIDEvent::digitizerEvent(now, 2,
        kIOHIDDigitizerTransducerTypeFinger, down, down ? 1 : 0, x, y);
    if (!child) { collection->release(); return NULL; }
    UInt32 mask = edge ? kIOHIDDigitizerEventFromEdgeTip : 0;
    if (was != down) mask |= kIOHIDDigitizerEventTouch | kIOHIDDigitizerEventRange;
    if (moved) mask |= kIOHIDDigitizerEventPosition;
    if (!was && down) mask |= kIOHIDDigitizerEventIdentity;
    child->setIntegerValue(kIOHIDEventFieldDigitizerIndex, 0);
    child->setIntegerValue(kIOHIDEventFieldDigitizerIdentity, 2);
    child->setIntegerValue(kIOHIDEventFieldDigitizerTouch, down);
    child->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, mask);
    collection->appendChild(child);
    child->release();
    if (!collection->getChildren() || collection->getChildren()->getCount() != 1) {
        collection->release(); return NULL;
    }
    collection->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerIsDisplayIntegrated, TRUE);
    collection->setIntegerValue(kIOHIDEventFieldIsBuiltIn, TRUE);
    collection->setFixedValue(kIOHIDEventFieldDigitizerX, x);
    collection->setFixedValue(kIOHIDEventFieldDigitizerY, y);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerRange, down);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerTouch, down);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, mask);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, down ? 1 : 0);
    return collection;
}

IOReturn
A04BR3HIDButtons::uiGated(OSObject *owner, void *arg, void *, void *, void *)
{
    A04BR3HIDButtons *self = (A04BR3HIDButtons *)owner;
    const uint64_t *s = (const uint64_t *)arg;
    if (!a04_ui_request_valid(A04_UI_SELECTOR, s, 3)) return kIOReturnBadArgument;
    if (!a04_ui_console()) return kIOReturnNotPermitted;
    if (self->fUIActive || self->fHeldKeys) return kIOReturnBusy;
    IOLockLock(gA04UIRegistryLock);
    A04BR3HIDTouch *touch = gA04BR3Touch;
    bool current = gA04BR3Buttons == self;
    if (touch) touch->retain();
    IOLockUnlock(gA04UIRegistryLock);
    if (!touch || !current) { OSSafeReleaseNULL(touch); return kIOReturnNotReady; }
    if (touch->uiWorkLoop() != self->_hidWorkLoop) {
        touch->release(); return kIOReturnNotReady;
    }
    if (touch->uiBusy()) { touch->release(); return kIOReturnBusy; }
    self->fUIActive = true;
    IOHIDEvent *down = NULL, *up = NULL;
    if (s[0] == A04_UI_HOME) {
        down = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDPage_Consumer, kHIDUsage_Csmr_Menu, true);
        up = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDPage_Consumer, kHIDUsage_Csmr_Menu, false);
    } else {
        IOFixed x = (IOFixed)((s[1] << 16) / 800), y = (IOFixed)((s[2] << 16) / 1280);
        bool edge = s[1] < 24 || s[1] > 775 || s[2] < 24 || s[2] > 1255;
        down = a04_ui_touch_event(true, false, true, edge, x, y);
        up = a04_ui_touch_event(false, true, false, edge, x, y);
    }
    IOReturn result = kIOReturnNoMemory;
    if (down && up) {
        // No allocations or userland roundtrip between the pair. A zero-length
        // press did not advance Setup Hello on hardware (Home and tap reached
        // the queue, UI unchanged) while physical presses do, so hold ~100ms
        // with the gate released; fUIActive keeps other UI requests busy.
        uint64_t deadline = 0;
        clock_interval_to_deadline(100, kMillisecondScale, &deadline);
        if (s[0] == A04_UI_HOME) {
            // Same path as the physical Home key (logs client count).
            const uint64_t press[2] = {kHIDUsage_Csmr_Menu, 1}, release[2] = {kHIDUsage_Csmr_Menu, 0};
            keyGated(self, (void *)press, NULL, NULL, NULL);
            self->_hidGate->commandSleep(&self->fUIActive, deadline, THREAD_UNINT);
            keyGated(self, (void *)release, NULL, NULL, NULL);
        } else {
            touch->uiDispatch(down);
            self->_hidGate->commandSleep(&self->fUIActive, deadline, THREAD_UNINT);
            up->setTimeStamp(mach_absolute_time());
            touch->uiDispatch(up);
        }
        result = kIOReturnSuccess;
    }
    OSSafeReleaseNULL(down);
    OSSafeReleaseNULL(up);
    self->fUIActive = false;
    touch->release();
    return result;
}

class A04BR3UITestClient : public IOUserClient {
    OSDeclareDefaultStructors(A04BR3UITestClient);
    volatile UInt32 fState; // bit0 request in flight, bit1 closed/dead
    A04BR3HIDButtons *fOwner; // remains valid through detach and in-flight close
public:
    bool initWithTask(task_t task, void *token, UInt32 type) APPLE_KEXT_OVERRIDE {
        fState = 0;
        fOwner = NULL;
        return IOUserClient::initWithTask(task, token, type);
    }
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE {
        if (!IOUserClient::start(provider)) return false;
        fOwner = OSDynamicCast(A04BR3HIDButtons, provider);
        if (!fOwner) return false;
        fOwner->retain();
        return true;
    }
    void free() APPLE_KEXT_OVERRIDE {
        OSSafeReleaseNULL(fOwner);
        IOUserClient::free();
    }
    void stop(IOService *provider) APPLE_KEXT_OVERRIDE {
        OSBitOrAtomic(2, &fState);
        IOUserClient::stop(provider);
    }
    IOReturn clientClose() APPLE_KEXT_OVERRIDE {
        OSBitOrAtomic(2, &fState);
        terminate();
        return kIOReturnSuccess;
    }
    IOReturn clientDied() APPLE_KEXT_OVERRIDE { return clientClose(); }
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *a,
        IOExternalMethodDispatch * = NULL, OSObject * = NULL, void * = NULL) APPLE_KEXT_OVERRIDE {
        if (!a || !a04_ui_request_valid(selector, a->scalarInput, a->scalarInputCount) ||
            a->structureInputSize || a->structureInputDescriptor || a->structureOutputSize ||
            a->structureOutputDescriptor || a->structureOutputDescriptorSize ||
            a->scalarOutputCount || a->asyncWakePort || a->asyncReferenceCount ||
            a->structureVariableOutputData) return kIOReturnBadArgument;
        if (!a04_ui_console() || clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
            return kIOReturnNotPermitted;
        if (!OSCompareAndSwap(0, 1, &fState)) return kIOReturnBusy;
        retain(); // close/death cannot destroy an in-flight finite pair
        A04BR3HIDButtons *provider = fOwner;
        uint64_t request[3] = {a->scalarInput[0], a->scalarInput[1], a->scalarInput[2]};
        IOReturn result = provider && !isInactive() ? provider->uiTest(request) : kIOReturnNotReady;
        OSCompareAndSwap(1, 0, &fState); // never reopen a closed client
        release();
        return result;
    }
};
OSDefineMetaClassAndStructors(A04BR3UITestClient, IOUserClient);

static IOReturn a04_ui_open(IOHIDEventService *provider, task_t task, void *token,
    UInt32 type, OSDictionary *, IOUserClient **handler)
{
    if (!handler) return kIOReturnBadArgument;
    *handler = NULL;
    if (type != A04_UI_CLIENT_TYPE || !OSDynamicCast(A04BR3HIDButtons, provider)) return kIOReturnUnsupported;
    if (!a04_ui_console() || IOUserClient::clientHasPrivilege(token, kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
        return kIOReturnNotPermitted;
    A04BR3UITestClient *client = OSTypeAlloc(A04BR3UITestClient);
    if (!client) return kIOReturnNoMemory;
    if (!client->initWithTask(task, token, type)) { client->release(); return kIOReturnBadArgument; }
    if (!client->attach(provider)) { client->release(); return kIOReturnUnsupported; }
    if (!client->start(provider)) { client->detach(provider); client->release(); return kIOReturnUnsupported; }
    *handler = client;
    return kIOReturnSuccess;
}
/* UI TEST END */


bool
A04BR3HIDButtons::start(IOService *provider)
{
    fHeldKeys = 0;
    fUIActive = false;
	if (!IOHIDEventService::start(provider)) {
		return false;
	}
    if (!a04_ui_registry_init()) return false;
	if (PE_parse_boot_argn("a04br3hidtest", &fTestSecs, sizeof(fTestSecs)) && fTestSecs) {
		fTest = IOTimerEventSource::timerEventSource(this, &A04BR3HIDButtons::testFired);
		if (fTest && _hidWorkLoop->addEventSource(fTest) == kIOReturnSuccess) {
			fTest->setTimeoutMS(fTestSecs * 1000);
		}
	}
	IOLockLock(gA04UIRegistryLock);
	gA04BR3Buttons = this;
	IOLockUnlock(gA04UIRegistryLock);
	registerService();
	IOLog("A04BR3 HID: buttons registered (test every %u s)\n", fTestSecs);
	a04br3_keys_init();
	return true;
}

void
A04BR3HIDButtons::key(UInt32 usage, bool down)
{
    uint64_t input[2] = {usage, down ? 1u : 0u};
    _hidGate->runAction(keyGated, input);
}

IOReturn
A04BR3HIDButtons::keyGated(OSObject *owner, void *arg, void *, void *, void *)
{
    A04BR3HIDButtons *self = (A04BR3HIDButtons *)owner;
    const uint64_t *s = (const uint64_t *)arg;
    UInt32 usage = (UInt32)s[0]; bool down = s[1] != 0;
    UInt32 bit = usage == kHIDUsage_Csmr_Menu ? 1 : usage == kHIDUsage_Csmr_Power ? 2 :
        usage == kHIDUsage_Csmr_VolumeIncrement ? 4 : usage == kHIDUsage_Csmr_VolumeDecrement ? 8 : 16;
    if (down) self->fHeldKeys |= bit; else self->fHeldKeys &= ~bit;
    IOHIDEvent *event = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDPage_Consumer, usage, down);
    if (event) {
        IOLog("A04BR3 HID: key 0x%x %s (%u clients)\n", (unsigned)usage, down ? "down" : "up",
            self->_clients ? self->_clients->getCount() : 0);
        self->dispatchEvent(event);
        event->release();
    }
    return kIOReturnSuccess;
}

void
A04BR3HIDButtons::testFired(OSObject *owner, IOTimerEventSource *sender)
{
	A04BR3HIDButtons *self = (A04BR3HIDButtons *)owner;

	/*
	 * Home (Menu) press, then the release 100 ms later; Setup's "Hello"
	 * screen waits for it. Even counts press, odd counts release.
	 */
	bool press = (self->fTestCount & 1) == 0;
	self->key(kHIDUsage_Csmr_Menu, press);
	if (++self->fTestCount < 60) {
		sender->setTimeoutMS(press ? 100 : self->fTestSecs * 1000);
	}
}

bool
A04BR3HIDTouch::start(IOService *provider)
{
	fLock = IOLockAlloc();
	if (!fLock || !IOHIDEventService::start(provider)) {
		return false;
	}
	if (!a04_ui_registry_init()) return false;
	/* The digitizer covers the main display (backboardd maps it 1:1). */
	setProperty("DisplayIntegrated", kOSBooleanTrue);
	setProperty("SurfaceWidth", (unsigned long long)(135 << 16), 32);	/* mm, 16.16 */
	setProperty("SurfaceHeight", (unsigned long long)(216 << 16), 32);
	IOLockLock(gA04UIRegistryLock);
	gA04BR3Touch = this;
	IOLockUnlock(gA04UIRegistryLock);
	registerService();
	IOLog("A04BR3 HID: touch registered\n");
	a04br3_touch_init();
	return true;
}

void
A04BR3HIDTouch::touch(int finger, bool down, int x, int y)
{
    int input[4] = {finger, down ? 1 : 0, x, y};
    _hidGate->runAction(touchGated, input);
}

IOReturn
A04BR3HIDTouch::touchGated(OSObject *owner, void *arg, void *, void *, void *)
{
    A04BR3HIDTouch *self = (A04BR3HIDTouch *)owner;
    const int *s = (const int *)arg;
    int finger = s[0]; bool down = s[1] != 0; int x = s[2], y = s[3];
    if (finger < 0 || finger >= A04BR3_TOUCH_FINGERS) return kIOReturnBadArgument;
    self->fHardwareDown = down; // also reject a real down whose event allocation failed
    x = x < 0 ? 0 : (x > 799 ? 799 : x);
    y = y < 0 ? 0 : (y > 1279 ? 1279 : y);
    IOLockLock(self->fLock);
    bool was = self->fDown[finger];
    IOFixed nx = (IOFixed)(((int64_t)x << 16) / 800);
    IOFixed ny = (IOFixed)(((int64_t)y << 16) / 1280);
    bool moved = down && (nx != self->fX[finger] || ny != self->fY[finger]);
    if (was == down && !moved) { IOLockUnlock(self->fLock); return kIOReturnSuccess; }
    bool edge = !was && down ? x < 24 || x > 775 || y < 24 || y > 1255 : self->fEdge[finger];
    IOHIDEvent *event = a04_ui_touch_event(down, was, moved, edge,
        down ? nx : self->fX[finger], down ? ny : self->fY[finger]);
    if (!event) { IOLockUnlock(self->fLock); return kIOReturnNoMemory; }
    // Preserve native collection position (child release uses last contact).
    event->setFixedValue(kIOHIDEventFieldDigitizerX, nx);
    event->setFixedValue(kIOHIDEventFieldDigitizerY, ny);
    self->fDown[finger] = down;
    self->fEdge[finger] = edge;
    if (down) { self->fX[finger] = nx; self->fY[finger] = ny; }
    IOLockUnlock(self->fLock);
    if (was != down) IOLog("A04BR3 HID: touch %d %s at %d,%d\n", finger, down ? "down" : "up", x, y);
    self->dispatchEvent(event);
    event->release();
    return kIOReturnSuccess;
}

IOReturn A04BR3HIDButtons::unpublish(OSObject *owner, void *, void *, void *, void *)
{
    IOLockLock(gA04UIRegistryLock);
    if (gA04BR3Buttons == owner) gA04BR3Buttons = NULL;
    IOLockUnlock(gA04UIRegistryLock);
    return kIOReturnSuccess;
}
void A04BR3HIDButtons::stop(IOService *provider)
{
    _hidGate->runAction(unpublish);
    if (fTest) { fTest->cancelTimeout(); _hidWorkLoop->removeEventSource(fTest); OSSafeReleaseNULL(fTest); }
    IOHIDEventService::stop(provider);
}
IOReturn A04BR3HIDTouch::unpublish(OSObject *owner, void *, void *, void *, void *)
{
    IOLockLock(gA04UIRegistryLock);
    if (gA04BR3Touch == owner) gA04BR3Touch = NULL;
    IOLockUnlock(gA04UIRegistryLock);
    return kIOReturnSuccess;
}
void A04BR3HIDTouch::stop(IOService *provider)
{
    _hidGate->runAction(unpublish);
    IOHIDEventService::stop(provider);
}

/* key: 0 power (side button), 1 volume up, 2 volume down, 3 home */
extern "C" void
a04br3_hid_key(int key, int down)
{
	static const UInt32 usages[] = {
		kHIDUsage_Csmr_Power, kHIDUsage_Csmr_VolumeIncrement,
		kHIDUsage_Csmr_VolumeDecrement, kHIDUsage_Csmr_Menu,
	};

    if (!gA04UIRegistryLock || key < 0 || key >= (int)(sizeof(usages) / sizeof(usages[0]))) return;
    IOLockLock(gA04UIRegistryLock);
    A04BR3HIDButtons *buttons = gA04BR3Buttons;
    if (buttons) buttons->retain();
    IOLockUnlock(gA04UIRegistryLock);
    if (buttons) { buttons->key(usages[key], down != 0); buttons->release(); }
}

extern "C" void
a04br3_hid_touch(int finger, int down, int x, int y)
{
    if (!gA04UIRegistryLock) return;
    IOLockLock(gA04UIRegistryLock);
    A04BR3HIDTouch *touch = gA04BR3Touch;
    if (touch) touch->retain();
    IOLockUnlock(gA04UIRegistryLock);
    if (touch) { touch->touch(finger, down != 0, x, y); touch->release(); }
}
