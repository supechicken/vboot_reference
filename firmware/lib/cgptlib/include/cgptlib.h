/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_CGPTLIB_H_
#define VBOOT_REFERENCE_CGPTLIB_H_

#include "2sysincludes.h"
#include "gpt_misc.h"

/**
 * Provides the location of the next bootable partition, in order of decreasing
 * priority.
 *
 * On return gpt.current_kernel contains the partition index of the current
 * bootable partition.
 *
 * Returns gpt entry of partition to boot if successful, else NULL
 */
GptEntry *GptNextKernelEntry(GptData *gpt);

/**
 * Checks if entry name field is equal to name+suffix.
 *
 * Returns 1 if equal, else 0.
 */
int GptEntryHasName(GptEntry *entry, const char *name,  const char *opt_suffix);

/**
 * Gets GPT entry for specified partition name and suffix.
 *
 * Returns pointer to GPT entry if successful, else NULL
 */
GptEntry *GptFindEntryByName(GptData *gpt, const char *name, const char *opt_suffix);

#endif  /* VBOOT_REFERENCE_CGPTLIB_H_ */
