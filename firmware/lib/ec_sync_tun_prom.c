/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PROM software sync (via EC passthrough) routines for vboot
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"

#include "sysincludes.h"
#include "ec_sync.h"
#include "gbb_header.h"
#include "vboot_common.h"
#include "vboot_kernel.h"

static void request_recovery(struct vb2_context *ctx, uint32_t recovery_request)
{
	VB2_DEBUG("request_recovery(%u)\n", recovery_request);

	vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST, recovery_request);
}

/**
 * Wrapper around VbExEcProtect() which sets recovery reason on error.
 */
static VbError_t protect_prom(struct vb2_context *ctx, int devidx)
{
	int rv = VbExEcProtect(devidx, 0);

	if (rv != VBERROR_SUCCESS) {
		VB2_DEBUG("VbExEcProtect() returned %d\n", rv);
		request_recovery(ctx, VB2_RECOVERY_EC_PROTECT);
	}
	return rv;
}

/**
 * Print a hash to debug output
 *
 * @param hash		Pointer to the hash
 * @param hash_size	Size of the hash in bytes
 * @param desc		Description of what's being hashed
 */
static void print_hash(const uint8_t *hash, uint32_t hash_size,
		       const char *desc)
{
	int i;

	VB2_DEBUG("%s hash: ", desc);
	for (i = 0; i < hash_size; i++)
		VB2_DEBUG_RAW("%02x", hash[i]);
	VB2_DEBUG_RAW("\n");
}

/**
 * Check if the hash of the PROM matches the expected hash.
 *
 * @param ctx		Vboot2 context
 * @param devidx	Index of PROM device to check
 * @return VB2_SUCCESS, or non-zero error code.
 */
static int check_prom_hash(struct vb2_context *ctx, int devidx,
			   int *fw_matched)
{
	*fw_matched = 1;

	/* Get current EC hash. */
	const uint8_t *ec_hash = NULL;
	int ec_hash_size;
	int rv = VbExEcHashImage(devidx, 0, &ec_hash, &ec_hash_size);
	if (rv) {
		VB2_DEBUG("VbExEcHashImage() returned %d\n", rv);
		request_recovery(ctx, VB2_RECOVERY_EC_HASH_FAILED);
		return VB2_ERROR_EC_HASH_IMAGE;
	}
	print_hash(ec_hash, ec_hash_size, "PROM");

	/* Get expected PROM hash. */
	const uint8_t *hash = NULL;
	int hash_size;
	rv = VbExEcGetExpectedImageHash(devidx, 0, &hash, &hash_size);
	if (rv) {
		VB2_DEBUG("VbExEcGetExpectedImageHash() returned %d\n", rv);
		request_recovery(ctx, VB2_RECOVERY_EC_EXPECTED_HASH);
		return VB2_ERROR_EC_HASH_EXPECTED;
	}
	if (ec_hash_size != hash_size) {
		VB2_DEBUG("PROM uses %d-byte hash, but update contains %d bytes\n",
			  ec_hash_size, hash_size);
		request_recovery(ctx, VB2_RECOVERY_EC_HASH_SIZE);
		return VB2_ERROR_EC_HASH_SIZE;
	}

	if (vb2_safe_memcmp(ec_hash, hash, hash_size)) {
		// we have some confidence that we can do an update
		print_hash(hash, hash_size, "Expected");
		*fw_matched = 0;
	}

	return VB2_SUCCESS;
}

/**
 * Update the specified PROM and verify the update succeeded
 *
 * @param ctx		Vboot2 context
 * @param devidx	Index of EC device to check
 * @return VBERROR_SUCCESS, or non-zero error code.
 */
static VbError_t update_prom(struct vb2_context *ctx, int devidx)
{
	// struct vb2_shared_data *sd = vb2_get_sd(ctx);
	enum VbSelectFirmware_t select = 0;

	VB2_DEBUG("updating %s...\n",
		  select == VB_SELECT_FIRMWARE_READONLY ? "RO" : "RW");

	/* Get expected EC image */
	const uint8_t *want = NULL;
	int want_size;
	int rv = VbExEcGetExpectedImage(devidx, select, &want, &want_size);
	if (rv) {
		VB2_DEBUG("VbExEcGetExpectedImage() returned %d\n", rv);
		request_recovery(ctx, VB2_RECOVERY_EC_EXPECTED_IMAGE);
		return rv;
	}
	VB2_DEBUG("image len = %d\n", want_size);

	rv = VbExEcUpdateImage(devidx, select, want, want_size);
	if (rv != VBERROR_SUCCESS) {
		VB2_DEBUG("VbExEcUpdateImage() returned %d\n", rv);

		/*
		 * The EC may know it needs a reboot.  It may need to
		 * unprotect the region before updating, or may need to
		 * reboot after updating.  Either way, it's not an error
		 * requiring recovery mode.
		 *
		 * If we fail for any other reason, trigger recovery
		 * mode.
		 */
		if (rv != VBERROR_EC_REBOOT_TO_RO_REQUIRED)
			request_recovery(ctx, VB2_RECOVERY_EC_UPDATE);

		return rv;
	}

	/* Verify the EC was updated properly */
#if 0
	sd->flags &= ~WHICH_EC(devidx, select);
#endif
	int fw_matched;
	if (check_prom_hash(ctx, devidx, &fw_matched) != VB2_SUCCESS)
		return VB2_ERROR_EC_HASH_EXPECTED;
	if (!fw_matched)
		return VB2_ERROR_EC_HASH_EXPECTED;
#if 0
	if (sd->flags & WHICH_EC(devidx, select)) {
		VB2_DEBUG("Failed to update\n");
		request_recovery(ctx, VB2_RECOVERY_EC_UPDATE);
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}
#endif
	return VBERROR_SUCCESS;
}



VbError_t ec_sync_phase_tun_proms(struct vb2_context *ctx, VbCommonParams *cparams)
{
	VbSharedDataHeader *shared =
		(VbSharedDataHeader *)cparams->shared_data_blob;
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	/* If we're not updating the EC, skip PROM syncs as well */
	if (!(shared->flags & VBSD_EC_SOFTWARE_SYNC))
		return VBERROR_SUCCESS;
	if (cparams->gbb->flags & GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC)
		return VBERROR_SUCCESS;
	if (sd->recovery_reason)
		return VBERROR_SUCCESS;

	// ...

	for (unsigned devidx = 0; /* ... */; ++devidx) {
		int fw_matched;

		// phase 1
		if (check_prom_hash(ctx, devidx, &fw_matched))
			return VB2_ERROR_EC_HASH_EXPECTED;
		if (fw_matched)
			continue;

		// phase 2

		// normally called by sync_one_ec

		VbError_t retval = update_prom(ctx, devidx);

		// should really skip to next PROM
		if (retval != VBERROR_SUCCESS)
			return retval;

		VbError_t rv = protect_prom(ctx, devidx);
		if (rv != VBERROR_SUCCESS)
			return rv;

	}

	return VBERROR_SUCCESS;
}
