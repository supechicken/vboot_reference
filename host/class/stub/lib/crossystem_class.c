/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Provides REVEN-specific system properties.
 */

#include "crossystem_class.h"

int VbGetClassPropertyInt(const char *name)
{
	return -1;
}

const char *VbGetClassPropertyString(const char *name, char *dest, size_t size)
{
	return NULL;
}

int VbSetClassPropertyInt(const char *name, int value)
{
	return -1;
}

int VbSetClassPropertyString(const char *name, const char *value) {
	return -1;
}
