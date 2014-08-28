/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Routines for verifying a kernel or disk image
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_common.h"
#include "util_misc.h"
#include "vboot_common.h"
#include "vboot_api.h"
#include "vboot_kernel.h"
#include "futility.h"

static uint8_t *diskbuf;
static uint64_t disk_bytes = 0;
static uint32_t stream_offset = 0;

static uint8_t shared_data[VB_SHARED_DATA_MIN_SIZE];
static VbSharedDataHeader *shared = (VbSharedDataHeader *)shared_data;
static VbNvContext nvc;

static LoadKernelParams params;
static VbCommonParams cparams;

VbError_t VbExDiskRead(VbExDiskHandle_t handle, uint64_t lba_start,
                       uint64_t lba_count, void *buffer)
{
	if (handle != (VbExDiskHandle_t)1)
		return VBERROR_UNKNOWN;
	if (lba_start > params.ending_lba)
		return VBERROR_UNKNOWN;
	if (lba_start + lba_count > params.ending_lba + 1)
		return VBERROR_UNKNOWN;

	memcpy(buffer, diskbuf + lba_start * 512, lba_count * 512);
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskWrite(VbExDiskHandle_t handle, uint64_t lba_start,
                        uint64_t lba_count, const void *buffer)
{
	if (handle != (VbExDiskHandle_t)1)
		return VBERROR_UNKNOWN;
	if (lba_start > params.ending_lba)
		return VBERROR_UNKNOWN;
	if (lba_start + lba_count > params.ending_lba + 1)
		return VBERROR_UNKNOWN;

	memcpy(diskbuf + lba_start * 512, buffer, lba_count * 512);
	return VBERROR_SUCCESS;
}

VbError_t VbExReadKernelStream(uint32_t bytes, void *buffer)
{
	/* Don't read past end of stream */
	if (bytes > disk_bytes || bytes + stream_offset > disk_bytes)
		return VBERROR_UNKNOWN;

	memcpy(buffer, diskbuf + stream_offset, bytes);
	stream_offset += bytes;
	return VBERROR_SUCCESS;
}

int do_verify_kernel(int argc, char *argv[])
{
	VbPublicKey *kernkey;
	int rv;

	const char *progname = strrchr(argv[0], '/');
	if (progname)
		progname++;
	else
		progname = argv[0];

	if (argc < 3) {
		fprintf(stderr,
			"usage: %s <disk_image> <kernel.vbpubk> [--stream]\n",
			progname);
		return 1;
	}

	/* Load disk file */
	/* TODO: better to nmap() in the long run */
	diskbuf = ReadFile(argv[1], &disk_bytes);
	if (!diskbuf) {
		fprintf(stderr, "Can't read disk file %s\n", argv[1]);
		return 1;
	}

	/* Read public key */
	kernkey = PublicKeyRead(argv[2]);
	if (!kernkey) {
		fprintf(stderr, "Can't read key file %s\n", argv[2]);
		return 1;
	}

	/* Set up shared data blob */
	VbSharedDataInit(shared, sizeof(shared_data));
	VbSharedDataSetKernelKey(shared, kernkey);
	/* TODO: optional TPM current kernel version */

	/* Set up params */
	memset(&params, 0, sizeof(params));
	params.shared_data_blob = shared_data;
	params.shared_data_size = sizeof(shared_data);

	/* GBB and cparams only needed by LoadKernel() in recovery mode */
	params.gbb_data = NULL;
	params.gbb_size = 0;
	memset(&cparams, 0, sizeof(cparams));

	/* TODO: optional dev-mode flag */
	params.boot_flags = 0;

	params.kernel_buffer_size = 16 * 1024 * 1024;
	params.kernel_buffer = malloc(params.kernel_buffer_size);
	if (!params.kernel_buffer) {
		fprintf(stderr, "Can't allocate kernel buffer\n");
		return 1;
	}

	/*
	 * LoadKernel() cares only about VBNV_DEV_BOOT_SIGNED_ONLY, and only in
	 * dev mode.  So just use defaults.
	 */
	VbNvSetup(&nvc);
	params.nv_context = &nvc;

	// TODO: better arg parsing
	if (argc > 3 && 0 == strcmp(argv[3], "--stream")) {
		/* Use stream mode */
		printf("Verifying in streaming mode.\n");
		params.boot_flags |= BOOT_FLAG_STREAMING;
	} else {
		/* Use image mode */
		printf("Verifying in image mode.\n");
		params.disk_handle = (VbExDiskHandle_t)1;
		params.bytes_per_lba = 512;
		params.ending_lba = disk_bytes / 512 - 1;
	}

	/* Try loading kernel */
	rv = LoadKernel(&params, &cparams);
	if (rv != VBERROR_SUCCESS) {
		fprintf(stderr, "LoadKernel() failed with code %d\n", rv);
		return 1;
	}

	printf("Found a good kernel.\n");
	printf("Partition number:   %d\n", (int)params.partition_number);
	printf("Bootloader address: 0x%" PRIx64 "\n",
	       params.bootloader_address);

	/* TODO: print other things (partition GUID, nv_context, shared_data) */

	printf("Yaay!\n");
	return 0;
}

DECLARE_FUTIL_COMMAND(verify_kernel, do_verify_kernel,
		      "Verifies a kernel / disk image");
