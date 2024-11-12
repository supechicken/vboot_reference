/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_CGPTLIB_H_
#define VBOOT_REFERENCE_CGPTLIB_H_

#include "2sysincludes.h"
#include "gpt_misc.h"

enum GptPartition {
	GPT_ANDROID_BOOT,
	GPT_ANDROID_INIT_BOOT,
	GPT_ANDROID_VENDOR_BOOT,
	GPT_ANDROID_PVMFW,
	GPT_ANDROID_MISC,
	/* Last entry, add new above */
	GPT_PART_MAX
};

extern const char *GptPartitionNames[GPT_PART_MAX];

/**
 * Provides the location of the next kernel partition, in order of decreasing
 * priority.
 *
 * On return the start_sector parameter contains the LBA sector for the start
 * of the kernel partition, and the size parameter contains the size of the
 * kernel partition in LBA sectors.  gpt.current_kernel contains the partition
 * index of the current chromeos kernel partition.
 *
 * Returns GPT_SUCCESS if successful, else
 *   GPT_ERROR_NO_VALID_KERNEL, no avaliable kernel, enters recovery mode */
int GptNextKernelEntry(GptData *gpt, uint64_t *start_sector, uint64_t *size);

/**
 * Get kernel partition suffix of active current_kernel.
 *
 * Returns suffix if successful, else NULL
 */
const char *GptGetActiveKernelPartitionSuffix(GptData *gpt);

/**
 * Provides start_sector and size for given partition by its UTF16LE name.
 *
 * On successful return the start sectore and size of partition.
 * Returns GPT_SUCCESS if successful, else GPT_ERROR_NO_SUCH_ENTRY.
 */
int GptFindPartitionOffset(GptData *gpt, const char *name,
			uint64_t *start_sector, uint64_t *size);

/**
 * Provides start_sector and size for active partition by its UTF16LE name.
 * It checks what is the active suffix (a/b) and adds it to name before
 * finding partition.
 *
 * On successful return the start sectore and size of partition.
 * Returns GPT_SUCCESS if successful, else GPT_ERROR_NO_SUCH_ENTRY.
 */
int GptFindActivePartitionOffset(GptData *gpt, const char *name,
			    uint64_t *start_sector, uint64_t *size);

/**
 * Find unique GUID for given partition name.
 *
 * On successful return the guid contains the unique GUID of partition.
 * Returns GPT_SUCCESS if successful, else GPT_ERROR_NO_SUCH_ENTRY.
 */
int GptFindPartitionUnique(GptData *gpt, const char *name, Guid *guid);

#endif  /* VBOOT_REFERENCE_CGPTLIB_H_ */
