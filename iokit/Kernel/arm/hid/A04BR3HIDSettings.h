/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2013 Apple Computer, Inc. All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/* Parameter-cache subset of IOHIDFamily-1915.140.3 IOHIDEventService.cpp
 * setSystemProperties()/setProperties(), and IOHIDevicePrivateKeys.h.
 * A04BR3 uses copy-on-write even for mutable previous dictionaries, so failed
 * merges cannot alter the previously published settings. No reset markers or
 * device-parameter envelope marker persist; no registry identity is changed.
 */
#ifndef A04BR3_HID_SETTINGS_H
#define A04BR3_HID_SETTINGS_H

static const char a04_hid_settings_key[] = "HIDEventServiceProperties";

/* Caller serializes the read/merge/publish transaction. Returned dictionary is
 * owned by caller; both input dictionaries remain unchanged. */
static OSDictionary *
a04_hid_settings_copy(const OSDictionary *previous, const OSDictionary *updates)
{
    if (!updates) return NULL;
    OSDictionary *next = OSDictionary::withDictionary(previous ? previous : updates);
    if (!next) return NULL;
    if (previous && !next->merge(updates)) {
        next->release();
        return NULL;
    }
    next->removeObject("HIDResetKeyboard");
    next->removeObject("HIDResetPointer");
    next->removeObject("HIDDeviceParameters");
    return next;
}
#endif
