/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions to load and verify an Android kernel.
 */

#ifndef VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_
#define VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_

#include "2api.h"
#include "cgptlib.h"
#include "gpt_misc.h"
#include "vboot_api.h"

struct android_part {
	uint8_t *buffer;
	size_t size;
};

extern struct android_part parts[];

/**
 * Reserves buffers forAndroid partitions (boot, init_boot, vendor_boot, pvmfw).
 *
 * @param params		Load-kernel parameters
 * @param gpt			Partition table from the disk
 * @param slot_suffix		Active partition slot suffix
 * @param disk_handle		Handle to the disk containing kernel
 * @return VB2_SUCCESS, or non-zero error code.
 */

vb2_error_t vboot_android_reserve_buffers(struct vb2_kernel_params *params, GptData *gpt,
	const char *slot_suffix, vb2ex_disk_handle_t disk_handle);

#endif  /* VBOOT_REFERENCE_2LOAD_ANDROID_KERNEL_H_ */
