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

/**
 * Gets address of buffer and size of preloaded partition.
 *
 * @param ops			AVB ops struct
 * @param name			Name of partition
 * @param buffer		Address of the pointer to buffer
 * @param data_size		Address of the partition size variable
 * @return AVB_IO_RESULT_OK on success or AVB_IO_RESULT_ERROR_IO otherwise.
 */
AvbIOResult vb2_android_get_buffer(AvbOps *ops,
				   enum GptPartition name,
				   void **buffer,
				   size_t *data_size);
/**
 * Loads and verifies Android partitions (boot, init_boot, vendor_boot, pvmfw).
 *
 * @param ctx			Vboot context
 * @param gpt			Partition table from the disk
 * @param entry			GPT entry with VBMETA partition
 * @param params		Load-kernel parameters
 * @param disk_handle		Handle to the disk containing kernel
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_load_android(
	struct vb2_context *ctx,
	GptData *gpt,
	GptEntry *entry,
	struct vb2_kernel_params *params,
	vb2ex_disk_handle_t disk_handle);

#endif  /* VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_ */
