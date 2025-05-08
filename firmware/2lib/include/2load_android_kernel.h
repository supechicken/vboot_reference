/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions to load and verify an Android kernel.
 */

#ifndef VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_
#define VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_

#include "2api.h"
#include "2avb.h"
#include "cgptlib.h"
#include "gpt_misc.h"
#include "vboot_api.h"

#define TIMESTAMP_2023_01_01 1672531200

static inline uint16_t android_timestamp_to_cros_version(uint64_t timestamp)
{
	return (timestamp > TIMESTAMP_2023_01_01 ?
		(timestamp - TIMESTAMP_2023_01_01) / (3600 * 24) :
		0);
}

static inline uint64_t cros_version_to_android_timestamp(uint16_t cros_version)
{
	return cros_version * 3600 * 24 + TIMESTAMP_2023_01_01;
}

/**
 * Loads Android vbmeta partitions and fetches version.
 * @param ctx			Vboot context
 * @param gpt			Partition table from the disk
 * @param entry			GPT entry with VBMETA partition
 * @param disk_info		Pointer to disk_info structure
 * @param kernel_version	Pointer to kernel version
 */
vb2_error_t vb2_get_android_version(struct vb2_context *ctx, GptData *gpt, GptEntry *entry,
				    struct vb2_disk_info *disk_info, uint32_t *kernel_version);

/**
 * Loads and verifies Android partitions (boot, init_boot, vendor_boot, pvmfw).
 *
 * @param ctx			Vboot context
 * @param gpt			Partition table from the disk
 * @param entry			GPT entry with VBMETA partition
 * @param params		Load-kernel parameters
 * @param disk_handle		Handle to the disk containing kernel
 * @param kernel_version	Pointer to kernel version
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_load_android(
	struct vb2_context *ctx,
	GptData *gpt,
	GptEntry *entry,
	struct vb2_kernel_params *params,
	vb2ex_disk_handle_t disk_handle,
	uint32_t *kernel_version);

#endif  /* VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_ */
