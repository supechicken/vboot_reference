/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions to load and verify an Android kernel.
 */

#include "2api.h"
#include "2avb.h"
#include "2common.h"
#include "2load_android_kernel.h"
#include "2misc.h"
#include "cgptlib.h"
#include "cgptlib_internal.h"
#include "gpt_misc.h"
#include "vboot_api.h"

static vb2_error_t vb2_load_android_partition(GptData *gpt, enum GptPartition part,
					      const char *suffix, uint8_t *buffer,
					      size_t buffer_size, vb2ex_disk_handle_t dh,
					      size_t *loaded)
{
	VbExStream_t stream;
	uint32_t read_ms = 0, start_ts;
	uint64_t part_start, part_size, part_bytes;
	vb2_error_t res = VB2_ERROR_LOAD_PARTITION_READ_BODY;
	GptEntry *e;

	e = GptFindEntryByName(gpt, GptPartitionNames[part], suffix);
	if (!e) {
		VB2_DEBUG("Unable to find %s%s\n", GptPartitionNames[part], suffix);
		return VB2_ERROR_LOAD_PARTITION_READ_BODY;
	}

	part_start = e->starting_lba;
	part_size = GptGetEntrySizeLba(e);

	if (VbExStreamOpen(dh, part_start, part_size, &stream)) {
		VB2_DEBUG("Unable to open disk handle.\n");
		return res;
	}

	part_bytes = GptGetEntrySizeBytes(gpt, e);
	if (part_bytes > buffer_size) {
		VB2_DEBUG("Buffer too small for partition %s%s (has %lu needs: %lld)\n",
			  GptPartitionNames[part], suffix, buffer_size, part_bytes);
		goto out;
	}

	/* Load partition to memory */
	start_ts = vb2ex_mtime();
	if (VbExStreamRead(stream, part_bytes, buffer)) {
		VB2_DEBUG("Unable to read android partition:%s%s\n",
			  GptPartitionNames[part], suffix);
		goto out;
	}
	read_ms = vb2ex_mtime() - start_ts;

	if (read_ms == 0)  /* Avoid division by 0 in speed calculation */
		read_ms = 1;
	VB2_DEBUG("read %u KB in %u ms at %u KB/s.\n", (uint32_t)part_bytes / 1024, read_ms,
		  (uint32_t)((part_bytes * VB2_MSEC_PER_SEC) / (read_ms * 1024)));

	if (loaded != NULL)
		*loaded = part_bytes;

	res = VB2_SUCCESS;
out:
	VbExStreamClose(stream);
	return res;
}


static int vboot_preload_partition(struct vb2_kernel_params *params, GptData *gpt,
				   const char *slot_suffix, vb2ex_disk_handle_t disk_handle)
{
	uint8_t *buf;
	uint32_t buf_size;
	size_t bytes_used = 0;
	vb2_error_t err;

	buf = params->kernel_buffer;
	buf_size = params->kernel_buffer_size;

	if (!buf || !buf_size) {
		VB2_DEBUG("Caller have not defined kernel_buffer and its size\n");
		return VB2_ERROR_LOAD_PARTITION_BODY_SIZE;
	}

	err = vb2_load_android_partition(gpt, GPT_ANDROID_BOOT, slot_suffix,
					 buf, buf_size, disk_handle, &bytes_used);
	if (err) {
		VB2_DEBUG("Cannot load boot partition\n");
		return err;
	}

	params->android_boot_size = bytes_used;
	buf_size -= bytes_used;
	buf += bytes_used;

	err = vb2_load_android_partition(gpt, GPT_ANDROID_VENDOR_BOOT, slot_suffix,
					 buf, buf_size, disk_handle, &bytes_used);
	if (err) {
		VB2_DEBUG("Cannot load vendor_boot partition\n");
		return err;
	}

	params->vendor_boot_buffer = buf;
	params->vendor_boot_size = bytes_used;

	buf_size -= bytes_used;
	buf += bytes_used;

	err= vb2_load_android_partition(gpt, GPT_ANDROID_INIT_BOOT, slot_suffix,
					buf, buf_size, disk_handle, &bytes_used);
	if (err) {
		VB2_DEBUG("Cannot load init_boot partition\n");
		return err;
	}

	params->init_boot_buffer = buf;
	params->init_boot_size = bytes_used;

	buf_size -= bytes_used;
	buf += bytes_used;

	return 0;
}
