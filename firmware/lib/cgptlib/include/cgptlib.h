/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_CGPTLIB_H_
#define VBOOT_REFERENCE_CGPTLIB_H_

#include "2sysincludes.h"
#include "gpt_misc.h"

/**
<<<<<<< HEAD   (7fcec0 2load_android_kernel: Clear misc partition boot command Andr)
||||||| BASE
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
=======
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
>>>>>>> CHANGE (5a32d0 vboot: modify GptNextKernelEntry function Android Bringup: S)
 * Find init_boot partition for selected slot.
 * Must be called after GptNextKernelEntry.
 *
 * On return the start_sector parameter contains the LBA sector for the start
 * of the init_boot partition, and the size parameter contains the size of the
 * init_boot partition in LBA sectors.
 * Returns GPT_SUCCESS if successful.
 */
int GptFindInitBoot(GptData *gpt, uint64_t *start_sector, uint64_t *size);

/**
 * Find vendor_boot partition for selected slot.
 * Must be called after GptNextKernelEntry.
 *
 * On return the start_sector parameter contains the LBA sector for the start
 * of the init_boot partition, and the size parameter contains the size of the
 * init_boot partition in LBA sectors.
 * Returns GPT_SUCCESS if successful.
 */
int GptFindVendorBoot(GptData *gpt, uint64_t *start_sector, uint64_t *size);

/**
 * Find pvmfw partition for selected slot.
 * Must be called after GptNextKernelEntry.
 *
 * On return the start_sector parameter contains the LBA sector for the start
 * of the pvmfw partition, and the size parameter contains the size of the
 * pvmfw partition in LBA sectors.
 * Returns GPT_SUCCESS if successful.
 */
int GptFindPvmfw(GptData *gpt, uint64_t *start_sector, uint64_t *size);

/**
 * Provides start_sector and size for given partition by its UTF16LE name.
 *
 * Returns GPT_SUCCESS if successful, else
 *   GPT_ERROR_NO_SUCH_ENTRY.
 */
int GptFindOffsetByName(GptData *gpt, const char *name,
			uint64_t *start_sector, uint64_t *size);

/**
 * Find unique GUID for given partition name.
 *
 * On successful return the guid contains the unique GUID of partition.
 * Returns GPT_SUCCESS if successful, else
 *   GPT_ERROR_NO_SUCH_ENTRY.
 */
int GptFindUniqueByName(GptData *gpt, const char *name, Guid *guid);

#endif  /* VBOOT_REFERENCE_CGPTLIB_H_ */
