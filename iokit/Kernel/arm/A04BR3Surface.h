/* Adapted from dipa-studio/ios-sweet fdad0de8dc0e75c5ea84762adc03f7232dc91f19,
 * patches 0046, 0056 and 0068. Experimental CPU display path for A04BR3. */
/*
 * Built-in IOSurface for A04BR3 (see A04BR3Surface.cpp).
 *
 * Shared with A04BR3Framebuffer.cpp: the framebuffer looks surfaces up by
 * id when a swap names them and publishes the panel memory itself as the
 * default framebuffer surface.
 */
#ifndef _A04BR3_SURFACE_H
#define _A04BR3_SURFACE_H

#include <pexpert/arm64/board_config.h>

#if defined(A04BR3)
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include <mach/mach_types.h>

#define A04BR3_SURFACE_MAX_PLANES        20

struct a04br3_surface_plane {
	uint32_t width, height, bytesPerRow, offset, size;
	uint16_t bytesPerElement;
	uint8_t  elementWidth, elementHeight;
};

class A04BR3Surface : public OSObject {
	OSDeclareDefaultStructors(A04BR3Surface);

public:
	uint32_t id;
	uint32_t width, height, bytesPerRow, offset, pixelFormat, allocSize;
	uint16_t bytesPerElement;
	uint8_t  elementWidth, elementHeight;
	uint32_t cacheMode, planeCount, parentID;
	struct a04br3_surface_plane planes[A04BR3_SURFACE_MAX_PLANES];

	IOMemoryDescriptor       *memory;       /* pixels (buffer or physical) */
	IOOptionBits              mapCache;     /* cache mode for user mappings */
	IOBufferMemoryDescriptor *state;        /* shared state page, read-only for users */
	IOMemoryMap              *kernelMap;    /* lazily created for kernel copies */

	bool init() APPLE_KEXT_OVERRIDE { return OSObject::init(); }
	void free() APPLE_KEXT_OVERRIDE;

	/* Kernel address of the pixels (mapped on first use), or 0. */
	uintptr_t kernelAddress();
};

/* Retained surface for an id, or NULL. */
A04BR3Surface *a04br3_surface_copy(uint32_t id);

/*
 * Publish physical memory (the panel framebuffer) as a surface. Returns a
 * retained surface that stays in the id table until released by the caller
 * and every user.
 */
A04BR3Surface *a04br3_surface_create_physical(uint64_t phys, uint32_t width,
    uint32_t height, uint32_t bytesPerRow, uint32_t pixelFormat);

/* Send right for the surface in task (IOSurfaceLookupFromMachPort). */
mach_port_name_t a04br3_surface_port_in_task(A04BR3Surface *surface, task_t task);

#endif /* A04BR3 */
#endif /* _A04BR3_SURFACE_H */
