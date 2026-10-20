/* Adapted from dipa-studio/ios-sweet fdad0de8dc0e75c5ea84762adc03f7232dc91f19,
 * patches 0046, 0056 and 0068. Experimental CPU display path for A04BR3. */
/*
 * Built-in IOSurface for A04BR3.
 *
 * Apple's IOSurface kext is closed. IOSurface.framework (iOS 16.7) finds
 * IOServiceNameMatching("IOCoreSurfaceRoot"), opens it with type 0 and
 * talks to the user client below:
 *
 *   sel 13  limits: struct out 0x18, u16 at +0x16 must be 1 (ABI version),
 *           +0 offset alignment mask, +4 bytes-per-row alignment mask
 *   sel 0   create: scalar [parent id], struct in = serialized property
 *           dictionary (binary or XML), struct out = lock result (0xa68)
 *   sel 4   lookup by id: scalar [id], struct out = lock result
 *   sel 34  lookup by mach port: scalar [port name], struct out = lock result
 *   sel 35  create mach port: scalars [id, options], scalar out [port name]
 *   traps   0/1 use count +/- (id, category), 2 lock / 3 unlock
 *           (id, options, &seed), 4/5 release (id)
 *
 * The lock result tells the library where the surface lives in the
 * caller's address space: +0 pixels, +8 shared state page (registry id,
 * seed at +0xc), +0x10 timestamps, then id, sizes and geometry (layout in
 * a04br3_surface_fill_result). Every surface is wired kernel memory mapped
 * into each task that creates or looks it up; mappings are counted per
 * client and dropped on release or close.
 *
 * Values, notifications, accelerators and shared events are accepted and
 * ignored (logged once per selector).
 */
#include "A04BR3Surface.h"

#if defined(A04BR3)
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/system.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSUnserialize.h>
#include <sys/proc.h>
#include <pexpert/pexpert.h>

enum {
	kA04BR3SurfaceCreate             = 0,
	kA04BR3SurfaceLookup             = 4,
	kA04BR3SurfaceLimits             = 13,
	kA04BR3SurfaceLookupFromPort     = 34,
	kA04BR3SurfaceCreatePort         = 35,
};

enum {
	kA04BR3SurfaceTrapIncrementUse   = 0,
	kA04BR3SurfaceTrapDecrementUse   = 1,
	kA04BR3SurfaceTrapLock           = 2,
	kA04BR3SurfaceTrapUnlock         = 3,
	kA04BR3SurfaceTrapRelease        = 4,
	kA04BR3SurfaceTrapReleaseFailed  = 5,
};

#define A04BR3_SURFACE_RESULT_SIZE       0xa68
#define A04BR3_SURFACE_MAX_ID            4096
#define A04BR3_SURFACE_MAX_DIM           16384
#define A04BR3_SURFACE_MAX_ALLOC         (256u << 20)
#define A04BR3_SURFACE_MAX_PROPS         (64u << 10)
#define A04BR3_SURFACE_ROW_ALIGN         64
#define A04BR3_SURFACE_TIMESTAMPS        0x800   /* offset in the state page */
#define A04BR3_SURFACE_STATE_SEED        0xc     /* u32 in the state page */
#define kIOSurfaceLockReadOnly          0x1

static IOLock       *a04br3_surface_lock;
static A04BR3Surface *a04br3_surface_table[A04BR3_SURFACE_MAX_ID];
static uint32_t      a04br3_surface_users[A04BR3_SURFACE_MAX_ID];
static uint32_t      a04br3_surface_next_id = 1;
static uint32_t      a04br3_surface_creates_logged;
static uint64_t      a04br3_surface_wired;       /* bytes of buffer surfaces */
#define A04BR3_SURFACE_WIRED_MAX         (512ull << 20)

OSDefineMetaClassAndStructors(A04BR3Surface, OSObject);

void
A04BR3Surface::free()
{
	if (OSDynamicCast(IOBufferMemoryDescriptor, memory) != NULL) {
		OSAddAtomic64(-(SInt64)allocSize, (volatile SInt64 *)&a04br3_surface_wired);
	}
	OSSafeReleaseNULL(kernelMap);
	OSSafeReleaseNULL(memory);
	OSSafeReleaseNULL(state);
	OSObject::free();
}

uintptr_t
A04BR3Surface::kernelAddress()
{
	IOBufferMemoryDescriptor *buffer = OSDynamicCast(IOBufferMemoryDescriptor, memory);
	if (buffer != NULL) {
		return (uintptr_t)buffer->getBytesNoCopy();
	}
	IOLockLock(a04br3_surface_lock);
	if (kernelMap == NULL && memory != NULL) {
		kernelMap = memory->map(kIOMapInhibitCache);
	}
	IOLockUnlock(a04br3_surface_lock);
	return kernelMap != NULL ? (uintptr_t)kernelMap->getVirtualAddress() : 0;
}

static uint32_t *
a04br3_surface_seed(A04BR3Surface *s)
{
	return (uint32_t *)((uint8_t *)s->state->getBytesNoCopy() + A04BR3_SURFACE_STATE_SEED);
}

/*
 * Give the surface an id and a table slot; the table holds one retain and a
 * user count (creators, client mappings, kernel holders). Lock held.
 */
static bool
a04br3_surface_insert_locked(A04BR3Surface *s)
{
	for (uint32_t n = 0; n < A04BR3_SURFACE_MAX_ID - 1; n++) {
		uint32_t id = a04br3_surface_next_id;
		a04br3_surface_next_id = id + 1 < A04BR3_SURFACE_MAX_ID ? id + 1 : 1;
		if (a04br3_surface_table[id] == NULL) {
			s->id = id;
			s->retain();
			a04br3_surface_table[id] = s;
			a04br3_surface_users[id] = 1;
			*(uint64_t *)s->state->getBytesNoCopy() = id;   /* registry id */
			return true;
		}
	}
	return false;
}

/* Add a user to a surface that may have left the table (port lookups). */
static bool
a04br3_surface_get(A04BR3Surface *s)
{
	bool ok = true;
	IOLockLock(a04br3_surface_lock);
	if (a04br3_surface_table[s->id] == s) {
		a04br3_surface_users[s->id]++;
	} else if (a04br3_surface_table[s->id] == NULL) {
		s->retain();
		a04br3_surface_table[s->id] = s;
		a04br3_surface_users[s->id] = 1;
	} else {
		ok = false;
	}
	IOLockUnlock(a04br3_surface_lock);
	return ok;
}

/* Drop a user; the last one removes the surface from the table. */
static void
a04br3_surface_put(A04BR3Surface *s)
{
	bool drop = false;
	IOLockLock(a04br3_surface_lock);
	if (a04br3_surface_table[s->id] == s && --a04br3_surface_users[s->id] == 0) {
		a04br3_surface_table[s->id] = NULL;
		drop = true;
	}
	IOLockUnlock(a04br3_surface_lock);
	if (drop) {
		s->release();
	}
}

A04BR3Surface *
a04br3_surface_copy(uint32_t id)
{
	A04BR3Surface *s = NULL;
	if (id == 0 || id >= A04BR3_SURFACE_MAX_ID || a04br3_surface_lock == NULL) {
		return NULL;
	}
	IOLockLock(a04br3_surface_lock);
	s = a04br3_surface_table[id];
	if (s != NULL) {
		s->retain();
	}
	IOLockUnlock(a04br3_surface_lock);
	return s;
}

/* The table lock; the framebuffer may publish a surface before the root starts. */
static void
a04br3_surface_init_lock(void)
{
	if (a04br3_surface_lock == NULL) {
		IOLock *lock = IOLockAlloc();
		if (!OSCompareAndSwapPtr(NULL, lock, &a04br3_surface_lock)) {
			IOLockFree(lock);
		}
	}
}

static A04BR3Surface *
a04br3_surface_alloc(void)
{
	a04br3_surface_init_lock();
	A04BR3Surface *s = OSTypeAlloc(A04BR3Surface);
	if (s == NULL || !s->init()) {
		OSSafeReleaseNULL(s);
		return NULL;
	}
	s->state = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
	    kIODirectionInOut | kIOMemoryKernelUserShared, page_size, page_size);
	if (s->state == NULL) {
		s->release();
		return NULL;
	}
	bzero(s->state->getBytesNoCopy(), page_size);
	return s;
}

static bool
a04br3_surface_publish(A04BR3Surface *s)
{
	bool ok;
	IOLockLock(a04br3_surface_lock);
	ok = a04br3_surface_insert_locked(s);
	IOLockUnlock(a04br3_surface_lock);
	return ok;
}

A04BR3Surface *
a04br3_surface_create_physical(uint64_t phys, uint32_t width, uint32_t height,
    uint32_t bytesPerRow, uint32_t pixelFormat)
{
	A04BR3Surface *s = a04br3_surface_alloc();
	if (s == NULL) {
		return NULL;
	}
	s->width = width;
	s->height = height;
	s->bytesPerRow = bytesPerRow;
	s->bytesPerElement = 4;
	s->elementWidth = s->elementHeight = 1;
	s->pixelFormat = pixelFormat;
	s->allocSize = (uint32_t)round_page(bytesPerRow * height);
	s->memory = IOMemoryDescriptor::withPhysicalAddress(phys, s->allocSize, kIODirectionInOut);
	s->mapCache = kIOMapInhibitCache;
	if (s->memory == NULL || !a04br3_surface_publish(s)) {
		s->release();
		return NULL;
	}
	return s;
}

mach_port_name_t
a04br3_surface_port_in_task(A04BR3Surface *s, task_t task)
{
	mach_port_name_t name = 0;
	IOUserClient::copyPortNameForObjectInTask(task, s, &name);
	return name;
}

static uint32_t
a04br3_dict_u32(OSDictionary *dict, const char *key, uint32_t def)
{
	OSNumber *n = OSDynamicCast(OSNumber, dict->getObject(key));
	return n != NULL ? n->unsigned32BitValue() : def;
}

static uint32_t
a04br3_div_up(uint32_t a, uint32_t b)
{
	return b > 1 ? (a + b - 1) / b : a;
}

static uint32_t
a04br3_row_bytes(uint32_t width, uint32_t ew, uint32_t bpe)
{
	uint64_t row = (uint64_t)a04br3_div_up(width, ew) * bpe;
	return (uint32_t)((row + A04BR3_SURFACE_ROW_ALIGN - 1) & ~(uint64_t)(A04BR3_SURFACE_ROW_ALIGN - 1));
}

/* Geometry from the creation properties (IOSurface.h keys). */
static IOReturn
a04br3_surface_layout(A04BR3Surface *s, OSDictionary *props)
{
	uint64_t end;

	s->width = a04br3_dict_u32(props, "IOSurfaceWidth", 0);
	s->height = a04br3_dict_u32(props, "IOSurfaceHeight", 0);
	s->bytesPerElement = (uint16_t)a04br3_dict_u32(props, "IOSurfaceBytesPerElement", 4);
	s->elementWidth = (uint8_t)a04br3_dict_u32(props, "IOSurfaceElementWidth", 1);
	s->elementHeight = (uint8_t)a04br3_dict_u32(props, "IOSurfaceElementHeight", 1);
	s->pixelFormat = a04br3_dict_u32(props, "IOSurfacePixelFormat", 0);
	s->cacheMode = a04br3_dict_u32(props, "IOSurfaceCacheMode", 0);
	s->offset = a04br3_dict_u32(props, "IOSurfaceOffset", 0);
	if (s->width > A04BR3_SURFACE_MAX_DIM || s->height > A04BR3_SURFACE_MAX_DIM ||
	    s->bytesPerElement > 64 || s->elementWidth == 0 || s->elementHeight == 0) {
		return kIOReturnBadArgument;
	}
	s->bytesPerRow = a04br3_dict_u32(props, "IOSurfaceBytesPerRow",
	    a04br3_row_bytes(s->width, s->elementWidth, s->bytesPerElement));
	if (s->bytesPerRow < (uint64_t)a04br3_div_up(s->width, s->elementWidth) * s->bytesPerElement) {
		return kIOReturnBadArgument;
	}
	end = (uint64_t)s->offset + (uint64_t)s->bytesPerRow * a04br3_div_up(s->height, s->elementHeight);

	OSArray *planes = OSDynamicCast(OSArray, props->getObject("IOSurfacePlaneInfo"));
	s->planeCount = 0;
	if (planes != NULL) {
		uint64_t next = s->offset;
		if (planes->getCount() > A04BR3_SURFACE_MAX_PLANES) {
			return kIOReturnBadArgument;
		}
		for (unsigned int i = 0; i < planes->getCount(); i++) {
			OSDictionary *pd = OSDynamicCast(OSDictionary, planes->getObject(i));
			struct a04br3_surface_plane *p = &s->planes[i];
			if (pd == NULL) {
				return kIOReturnBadArgument;
			}
			p->width = a04br3_dict_u32(pd, "IOSurfacePlaneWidth", s->width);
			p->height = a04br3_dict_u32(pd, "IOSurfacePlaneHeight", s->height);
			p->bytesPerElement = (uint16_t)a04br3_dict_u32(pd, "IOSurfacePlaneBytesPerElement", 1);
			p->elementWidth = (uint8_t)a04br3_dict_u32(pd, "IOSurfacePlaneElementWidth", 1);
			p->elementHeight = (uint8_t)a04br3_dict_u32(pd, "IOSurfacePlaneElementHeight", 1);
			if (p->width > A04BR3_SURFACE_MAX_DIM || p->height > A04BR3_SURFACE_MAX_DIM ||
			    p->bytesPerElement > 64 || p->elementWidth == 0 || p->elementHeight == 0) {
				return kIOReturnBadArgument;
			}
			p->bytesPerRow = a04br3_dict_u32(pd, "IOSurfacePlaneBytesPerRow",
			    a04br3_row_bytes(p->width, p->elementWidth, p->bytesPerElement));
			p->offset = a04br3_dict_u32(pd, "IOSurfacePlaneOffset", (uint32_t)next);
			p->size = a04br3_dict_u32(pd, "IOSurfacePlaneSize",
			    p->bytesPerRow * a04br3_div_up(p->height, p->elementHeight));
			next = round_page((uint64_t)p->offset + p->size);
			if ((uint64_t)p->offset + p->size > end) {
				end = (uint64_t)p->offset + p->size;
			}
		}
		s->planeCount = planes->getCount();
	}

	uint64_t alloc = a04br3_dict_u32(props, "IOSurfaceAllocSize", 0);
	if (alloc < end) {
		alloc = end;
	}
	alloc = round_page(alloc);
	if (alloc == 0 || alloc > A04BR3_SURFACE_MAX_ALLOC) {
		return kIOReturnNoMemory;
	}
	s->allocSize = (uint32_t)alloc;
	return kIOReturnSuccess;
}

/* Per-client mapping of one surface. */
class A04BR3SurfaceRef : public OSObject {
	OSDeclareDefaultStructors(A04BR3SurfaceRef);

public:
	A04BR3Surface *surface;
	IOMemoryMap  *data;
	IOMemoryMap  *state;
	uint32_t      count;

	bool init() APPLE_KEXT_OVERRIDE { return OSObject::init(); }
	void free() APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(A04BR3SurfaceRef, OSObject);

void
A04BR3SurfaceRef::free()
{
	OSSafeReleaseNULL(data);
	OSSafeReleaseNULL(state);
	if (surface != NULL) {
		a04br3_surface_put(surface);
		surface->release();
		surface = NULL;
	}
	OSObject::free();
}

class IOCoreSurfaceRoot : public IOService {
	OSDeclareDefaultStructors(IOCoreSurfaceRoot);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IOCoreSurfaceRoot, IOService);

bool
IOCoreSurfaceRoot::start(IOService *provider)
{
	unsigned enabled = 0;
	if (!PE_parse_boot_argn("a04br3.display_probe", &enabled, sizeof(enabled)) || enabled != 1) return false;
	if (!IOService::start(provider)) {
		return false;
	}
	a04br3_surface_init_lock();
	setProperty(kIOUserClientClassKey, "IOSurfaceRootUserClient");
	registerService();
	IOLog("A04BR3 SURF: IOCoreSurfaceRoot registered\n");
	return true;
}

class IOSurfaceRootUserClient : public IOUserClient {
	OSDeclareDefaultStructors(IOSurfaceRootUserClient);

	task_t   fTask;
	IOLock  *fLock;
	OSArray *fRefs;

	A04BR3SurfaceRef *findRef(uint32_t id, unsigned int *index);
	IOReturn addRef(A04BR3Surface *s, A04BR3SurfaceRef **refOut, bool *existing);
	IOReturn returnSurface(A04BR3Surface *s, IOExternalMethodArguments *args);
	IOReturn create(IOExternalMethodArguments *args);

public:
	bool initWithTask(task_t owningTask, void *securityID, UInt32 type) APPLE_KEXT_OVERRIDE;
	void free() APPLE_KEXT_OVERRIDE;
	IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target,
	    void *reference) APPLE_KEXT_OVERRIDE;
	IOExternalTrap *getTargetAndTrapForIndex(IOService **targetP, UInt32 index) APPLE_KEXT_OVERRIDE;

	IOReturn trapUseCount(void *id, void *category, void *, void *, void *, void *);
	IOReturn trapLock(void *id, void *options, void *seedOut, void *, void *, void *);
	IOReturn trapUnlock(void *id, void *options, void *seedOut, void *, void *, void *);
	IOReturn trapRelease(void *id, void *, void *, void *, void *, void *);
};

OSDefineMetaClassAndStructors(IOSurfaceRootUserClient, IOUserClient);

static uint64_t a04br3_surf_logged;      /* selectors / traps already logged (< 64) */

static void
a04br3_surf_log_once(const char *what, uint32_t n)
{
	uint32_t bit = (what[0] == 't' ? 48 : 0) + n;
	if (bit >= 64 || (a04br3_surf_logged & (1ULL << bit))) {
		return;
	}
	a04br3_surf_logged |= 1ULL << bit;
	char name[32];
	proc_selfname(name, sizeof(name));
	IOLog("A04BR3 SURF: %s %s %u\n", name, what, n);
}

bool
IOSurfaceRootUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	if (!IOUserClient::initWithTask(owningTask, securityID, type)) {
		return false;
	}
	fTask = owningTask;
	fLock = IOLockAlloc();
	fRefs = OSArray::withCapacity(16);
	return fLock != NULL && fRefs != NULL;
}

void
IOSurfaceRootUserClient::free()
{
	OSSafeReleaseNULL(fRefs);
	if (fLock != NULL) {
		IOLockFree(fLock);
		fLock = NULL;
	}
	IOUserClient::free();
}

IOReturn
IOSurfaceRootUserClient::clientClose(void)
{
	IOLockLock(fLock);
	fRefs->flushCollection();
	IOLockUnlock(fLock);
	terminate();
	return kIOReturnSuccess;
}

/* Lock held. */
A04BR3SurfaceRef *
IOSurfaceRootUserClient::findRef(uint32_t id, unsigned int *index)
{
	for (unsigned int i = 0; i < fRefs->getCount(); i++) {
		A04BR3SurfaceRef *ref = (A04BR3SurfaceRef *)fRefs->getObject(i);
		if (ref->surface->id == id) {
			if (index != NULL) {
				*index = i;
			}
			return ref;
		}
	}
	return NULL;
}

/*
 * Count one more create/lookup of s by this client, mapping it into the
 * task the first time. Takes over the caller's user count on s.
 *
 * A lookup of a surface this client already has maps nothing and counts
 * nothing (*existing): IOSurface.framework then gets base address -1 in the
 * lock result and reuses its cached IOSurfaceClient. Counting it instead
 * made the framework build a second client for the same id; releasing that
 * one (traps 4 + 5) unmapped the memory under the first (backboardd
 * snapshot crash in CGDataProviderCreateWithData).
 */
IOReturn
IOSurfaceRootUserClient::addRef(A04BR3Surface *s, A04BR3SurfaceRef **refOut, bool *existing)
{
	*existing = false;
	IOLockLock(fLock);
	A04BR3SurfaceRef *ref = findRef(s->id, NULL);
	if (ref != NULL && ref->surface == s) {
		IOLockUnlock(fLock);
		a04br3_surface_put(s);
		*refOut = ref;
		*existing = true;
		return kIOReturnSuccess;
	}
	IOLockUnlock(fLock);

	ref = OSTypeAlloc(A04BR3SurfaceRef);
	if (ref == NULL || !ref->init()) {
		OSSafeReleaseNULL(ref);
		a04br3_surface_put(s);
		return kIOReturnNoMemory;
	}
	s->retain();
	ref->surface = s;
	ref->count = 1;
	ref->data = s->memory->createMappingInTask(fTask, 0, kIOMapAnywhere | s->mapCache);
	ref->state = s->state->createMappingInTask(fTask, 0, kIOMapAnywhere | kIOMapReadOnly);
	if (ref->data == NULL || ref->state == NULL) {
		ref->release();
		return kIOReturnVMError;
	}
	IOLockLock(fLock);
	A04BR3SurfaceRef *raced = findRef(s->id, NULL);
	if (raced != NULL && raced->surface == s) {
		/* Another thread mapped it meanwhile: count there, drop ours. */
		raced->count++;
		IOLockUnlock(fLock);
		ref->release();
		*refOut = raced;
		return kIOReturnSuccess;
	}
	fRefs->setObject(ref);
	IOLockUnlock(fLock);
	ref->release();         /* fRefs holds it */
	*refOut = ref;
	return kIOReturnSuccess;
}

static void
put32(uint8_t *p, uint32_t off, uint32_t v)
{
	memcpy(p + off, &v, 4);
}

static void
put64(uint8_t *p, uint32_t off, uint64_t v)
{
	memcpy(p + off, &v, 8);
}

/* IOSurface.framework's lock result (copied to IOSurfaceClient + 0x68). */
static void
a04br3_surface_fill_result(A04BR3Surface *s, A04BR3SurfaceRef *ref, uint8_t *out)
{
	bzero(out, A04BR3_SURFACE_RESULT_SIZE);
	put64(out, 0x00, ref->data->getAddress());
	put64(out, 0x08, ref->state->getAddress());
	put64(out, 0x10, ref->state->getAddress() + A04BR3_SURFACE_TIMESTAMPS);
	put32(out, 0x18, s->id);
	put32(out, 0x1c, s->allocSize);
	put32(out, 0x20, s->width);
	put32(out, 0x24, s->height);
	put32(out, 0x28, s->bytesPerRow);
	put32(out, 0x2c, s->offset);
	put32(out, 0x30, s->pixelFormat);
	put32(out, 0x34, s->allocSize);
	put32(out, 0x38, s->planeCount);
	memcpy(out + 0x40, &s->bytesPerElement, 2);
	out[0x42] = s->elementWidth;
	out[0x43] = s->elementHeight;
	put32(out, 0x44, s->cacheMode);
	put32(out, 0x50, s->parentID);
	put32(out, 0x54, 1);
	for (uint32_t i = 0; i < s->planeCount; i++) {
		uint8_t *p = out + 0x68 + i * 0x80;
		const struct a04br3_surface_plane *pl = &s->planes[i];
		put32(p, 0x00, pl->width);
		put32(p, 0x04, pl->height);
		put32(p, 0x08, pl->bytesPerRow);
		put32(p, 0x0c, pl->offset);
		put32(p, 0x14, pl->size);
		memcpy(p + 0x18, &pl->bytesPerElement, 2);
		p[0x1a] = pl->elementWidth;
		p[0x1b] = pl->elementHeight;
	}
}

static IOReturn
a04br3_struct_out(IOExternalMethodArguments *args, const void *buf, uint32_t len)
{
	if (args->structureOutputDescriptor != NULL) {
		if (len > args->structureOutputDescriptor->getLength()) {
			return kIOReturnNoSpace;
		}
		args->structureOutputDescriptor->writeBytes(0, buf, len);
		args->structureOutputDescriptorSize = len;
		return kIOReturnSuccess;
	}
	if (args->structureOutput == NULL || len > args->structureOutputSize) {
		return kIOReturnNoSpace;
	}
	memcpy(args->structureOutput, buf, len);
	args->structureOutputSize = len;
	return kIOReturnSuccess;
}

/* Map s for this client and answer with its lock result. Takes s's user count. */
IOReturn
IOSurfaceRootUserClient::returnSurface(A04BR3Surface *s, IOExternalMethodArguments *args)
{
	A04BR3SurfaceRef *ref = NULL;
	bool existing = false;
	IOReturn ret = addRef(s, &ref, &existing);
	if (ret != kIOReturnSuccess) {
		return ret;
	}
	uint8_t *out = (uint8_t *)IOMallocZeroData(A04BR3_SURFACE_RESULT_SIZE);
	if (out == NULL) {
		ret = kIOReturnNoMemory;
	} else {
		a04br3_surface_fill_result(s, ref, out);
		if (existing) {
			put64(out, 0x00, ~0ULL);        /* "already yours": use the cached client */
		}
		ret = a04br3_struct_out(args, out, A04BR3_SURFACE_RESULT_SIZE);
		IOFreeData(out, A04BR3_SURFACE_RESULT_SIZE);
	}
	if (ret != kIOReturnSuccess && !existing) {
		/* The caller never learns the id: undo this create/lookup. */
		trapRelease((void *)(uintptr_t)s->id, NULL, NULL, NULL, NULL, NULL);
	}
	return ret;
}

IOReturn
IOSurfaceRootUserClient::create(IOExternalMethodArguments *args)
{
	const char *buf = (const char *)args->structureInput;
	uint32_t len = args->structureInputSize;
	char *copy = NULL;
	IOReturn ret;

	if (args->structureInputDescriptor != NULL) {
		len = (uint32_t)args->structureInputDescriptor->getLength();
		if (len == 0 || len > A04BR3_SURFACE_MAX_PROPS) {
			return kIOReturnBadArgument;
		}
		copy = (char *)IOMallocData(len);
		if (copy == NULL) {
			return kIOReturnNoMemory;
		}
		args->structureInputDescriptor->readBytes(0, copy, len);
		buf = copy;
	}
	OSObject *obj = (buf != NULL && len > 0) ? OSUnserializeXML(buf, len) : NULL;
	if (copy != NULL) {
		IOFreeData(copy, len);
	}
	OSDictionary *props = OSDynamicCast(OSDictionary, obj);
	if (props == NULL) {
		OSSafeReleaseNULL(obj);
		IOLog("A04BR3 SURF: create: bad property dictionary (%u bytes)\n", len);
		return kIOReturnBadArgument;
	}

	A04BR3Surface *s = a04br3_surface_alloc();
	if (s == NULL) {
		props->release();
		return kIOReturnNoMemory;
	}
	s->parentID = args->scalarInputCount > 0 ? (uint32_t)args->scalarInput[0] : 0;
	ret = a04br3_surface_layout(s, props);
	if (ret == kIOReturnSuccess) {
		IOBufferMemoryDescriptor *mem = NULL;
		if ((uint64_t)OSAddAtomic64(s->allocSize, (volatile SInt64 *)&a04br3_surface_wired) +
		    s->allocSize <= A04BR3_SURFACE_WIRED_MAX) {
			mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
			    kIODirectionInOut | kIOMemoryKernelUserShared, s->allocSize, page_size);
		}
		if (mem == NULL) {
			OSAddAtomic64(-(SInt64)s->allocSize, (volatile SInt64 *)&a04br3_surface_wired);
			ret = kIOReturnNoMemory;
		} else {
			bzero(mem->getBytesNoCopy(), s->allocSize);
			s->memory = mem;
			s->mapCache = kIOMapDefaultCache;
			if (!a04br3_surface_publish(s)) {
				ret = kIOReturnNoResources;
			}
		}
	}
	if (ret != kIOReturnSuccess || a04br3_surface_creates_logged < 64) {
		char name[32];
		proc_selfname(name, sizeof(name));
		a04br3_surface_creates_logged++;
		IOLog("A04BR3 SURF: %s create id %u %ux%u fmt 0x%x bpe %u bpr %u planes %u size %u keys %u -> 0x%x\n",
		    name, s->id, s->width, s->height, s->pixelFormat, s->bytesPerElement,
		    s->bytesPerRow, s->planeCount, s->allocSize, props->getCount(), ret);
	}
	props->release();
	if (ret != kIOReturnSuccess) {
		s->release();
		return ret;
	}
	ret = returnSurface(s, args);   /* takes the creator's user count */
	s->release();
	return ret;
}

IOReturn
IOSurfaceRootUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    __unused IOExternalMethodDispatch *dispatch, __unused OSObject *target,
    __unused void *reference)
{
	uint64_t arg0 = args->scalarInputCount > 0 ? args->scalarInput[0] : 0;

	switch (selector) {
	case kA04BR3SurfaceLimits: {
		uint8_t limits[0x18] = { 0 };
		put32(limits, 0x0, A04BR3_SURFACE_ROW_ALIGN - 1);
		put32(limits, 0x4, A04BR3_SURFACE_ROW_ALIGN - 1);
		limits[0x16] = 1;
		a04br3_surf_log_once("sel", selector);
		return a04br3_struct_out(args, limits, sizeof(limits));
	}
	case kA04BR3SurfaceCreate:
		return create(args);
	case kA04BR3SurfaceLookup: {
		A04BR3Surface *s = a04br3_surface_copy((uint32_t)arg0);
		IOReturn ret = kIOReturnNotFound;
		if (s != NULL) {
			ret = a04br3_surface_get(s) ? returnSurface(s, args) : kIOReturnNotFound;
			s->release();
		}
		a04br3_surf_log_once("sel", selector);
		return ret;
	}
	case kA04BR3SurfaceLookupFromPort: {
		OSObject *obj = NULL;
		IOReturn ret = IOUserClient::copyObjectForPortNameInTask(fTask, (mach_port_name_t)arg0, &obj);
		A04BR3Surface *s = OSDynamicCast(A04BR3Surface, obj);
		if (ret == kIOReturnSuccess) {
			ret = (s != NULL && a04br3_surface_get(s)) ? returnSurface(s, args) : kIOReturnBadArgument;
		}
		OSSafeReleaseNULL(obj);
		a04br3_surf_log_once("sel", selector);
		return ret;
	}
	case kA04BR3SurfaceCreatePort: {
		A04BR3Surface *s = a04br3_surface_copy((uint32_t)arg0);
		if (s == NULL || args->scalarOutputCount < 1) {
			OSSafeReleaseNULL(s);
			return kIOReturnNotFound;
		}
		args->scalarOutput[0] = a04br3_surface_port_in_task(s, fTask);
		args->scalarOutputCount = 1;
		s->release();
		a04br3_surf_log_once("sel", selector);
		return kIOReturnSuccess;
	}
	default:
		/* Values, notifications, ownership, timestamps...: accept and ignore. */
		for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
			args->scalarOutput[i] = 0;
		}
		if (args->structureOutput != NULL) {
			bzero(args->structureOutput, args->structureOutputSize);
		}
		a04br3_surf_log_once("sel", selector);
		return kIOReturnSuccess;
	}
}

IOReturn
IOSurfaceRootUserClient::trapUseCount(__unused void *id, __unused void *category,
    void *, void *, void *, void *)
{
	return kIOReturnSuccess;
}

static IOReturn
a04br3_surface_seed_out(A04BR3Surface *s, void *seedOut, bool bump)
{
	uint32_t seed = bump ? OSIncrementAtomic((volatile SInt32 *)a04br3_surface_seed(s)) + 1 :
	    *a04br3_surface_seed(s);
	if (seedOut != NULL && copyout(&seed, (user_addr_t)seedOut, sizeof(seed)) != 0) {
		return kIOReturnBadArgument;
	}
	return kIOReturnSuccess;
}

/* Retained surface this client created or looked up, or NULL. */
static A04BR3Surface *
a04br3_surface_held(IOLock *lock, OSArray *refs, uint32_t id)
{
	A04BR3Surface *s = NULL;
	IOLockLock(lock);
	for (unsigned int i = 0; i < refs->getCount(); i++) {
		A04BR3SurfaceRef *ref = (A04BR3SurfaceRef *)refs->getObject(i);
		if (ref->surface->id == id) {
			s = ref->surface;
			s->retain();
			break;
		}
	}
	IOLockUnlock(lock);
	return s;
}

IOReturn
IOSurfaceRootUserClient::trapLock(void *id, __unused void *options, void *seedOut,
    void *, void *, void *)
{
	A04BR3Surface *s = a04br3_surface_held(fLock, fRefs, (uint32_t)(uintptr_t)id);
	if (s == NULL) {
		return kIOReturnNotFound;
	}
	IOReturn ret = a04br3_surface_seed_out(s, seedOut, false);
	s->release();
	return ret;
}

IOReturn
IOSurfaceRootUserClient::trapUnlock(void *id, void *options, void *seedOut,
    void *, void *, void *)
{
	A04BR3Surface *s = a04br3_surface_held(fLock, fRefs, (uint32_t)(uintptr_t)id);
	if (s == NULL) {
		return kIOReturnNotFound;
	}
	IOReturn ret = a04br3_surface_seed_out(s, seedOut,
	    !((uintptr_t)options & kIOSurfaceLockReadOnly));
	s->release();
	return ret;
}

IOReturn
IOSurfaceRootUserClient::trapRelease(void *id, void *, void *, void *, void *, void *)
{
	unsigned int index;
	IOLockLock(fLock);
	A04BR3SurfaceRef *ref = findRef((uint32_t)(uintptr_t)id, &index);
	if (ref != NULL && --ref->count == 0) {
		fRefs->removeObject(index);     /* unmaps and drops the user count */
	}
	IOLockUnlock(fLock);
	return ref != NULL ? kIOReturnSuccess : kIOReturnNotFound;
}

static IOExternalTrap a04br3_surface_traps[] = {
	[kA04BR3SurfaceTrapIncrementUse]  = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapUseCount },
	[kA04BR3SurfaceTrapDecrementUse]  = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapUseCount },
	[kA04BR3SurfaceTrapLock]          = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapLock },
	[kA04BR3SurfaceTrapUnlock]        = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapUnlock },
	[kA04BR3SurfaceTrapRelease]       = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapRelease },
	[kA04BR3SurfaceTrapReleaseFailed] = { NULL, (IOTrap) & IOSurfaceRootUserClient::trapRelease },
};

IOExternalTrap *
IOSurfaceRootUserClient::getTargetAndTrapForIndex(IOService **targetP, UInt32 index)
{
	if (index >= sizeof(a04br3_surface_traps) / sizeof(a04br3_surface_traps[0])) {
		a04br3_surf_log_once("trap", index);
		return NULL;
	}
	a04br3_surf_log_once("trap", index);
	*targetP = this;
	return &a04br3_surface_traps[index];
}
#endif /* A04BR3 */
