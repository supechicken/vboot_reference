/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Interfaces to provide overridden for certain system properties for
 * specific class of ChromeOS devices.
 */

#ifndef VBOOT_REFERENCE_CROSSYSTEM_CLASS_H_
#define VBOOT_REFERENCE_CROSSYSTEM_CLASS_H_

#include <stddef.h>

/* Apis WITH CLASS-OVERRIDDEN IMPLEMENTATIONS */

/* Read an class-overridden system property integer.
 *
 * Returns the property value, or -1 if error. */
int VbGetClassPropertyInt(const char* name);

/* Read an class-overridden system property string into a destination
 * buffer of the specified size.  Returned string will be
 * null-terminated.  If the buffer is too small, the returned string
 * will be truncated.
 *
 * Returns the passed buffer, or NULL if error. */
const char* VbGetClassPropertyString(const char* name, char* dest, size_t size);

/* Set an class-overridden system property integer.
 *
 * Returns 0 if success, -1 if error. */
int VbSetClassPropertyInt(const char* name, int value);

/* Set an class-overridden system property string.
 *
 * Returns 0 if success, -1 if error. */
int VbSetClassPropertyString(const char* name, const char* value);

#endif  /* VBOOT_REFERENCE_CROSSYSTEM_CLASS_H_ */
