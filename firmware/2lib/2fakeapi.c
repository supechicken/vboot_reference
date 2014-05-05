/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Fake / mock API implementations
 */

#include "2sysincludes.h"
#include "2api.h"

// TODO: clunky that recovery reasons are in an internal header; should move
// them to the API
#include "vboot_nvstorage.h"

//#include "vboot_api.h"

int vb2ex_tpm_clear_owner(struct vb2_context *ctx)
{
	// TODO: implement
	return VB2_SUCCESS;
}

int vb2ex_read_resource(struct vb2_context *ctx,
			enum vb2_resource_index index,
			uint32_t offset,
			void *buf,
			uint32_t *size)
{
	// TODO: implement
	return VB2_SUCCESS;
}


////////////////////////////////////////////////////////////////////

/* Fake functions from elsewhere */

/* Read and save non-volatile storage data */
void fake_read_nvdata(uint8_t *nvdata) {}
void fake_save_nvdata(uint8_t *nvdata) {}

/* Read and save secure storage data */
void fake_read_secdata(uint8_t *secdata) {}
void fake_save_secdata(uint8_t *secdata) {}
/* Lock secure storage data */
void fake_lock_secdata(void) {}

/* Read recovery button (real, or event from EC) */
int fake_read_recovery_button(void) { return 0; }

/* Read hardware developer switch */
int fake_read_hw_dev_switch(void) { return 0; }

/* Read firmware body; dest_ptr will point to data read; returns how much */
uint32_t fake_read_next_body(uint8_t **dest_ptr) { *dest_ptr = NULL; return 0; }

/* Jump to recovery mode firmware */
int fake_boot_to_recovery_mode(void) { return 0; }

/* Reboot */
int fake_reboot(void) { return 0; }

////////////////////////////////////////////////////////////////////

/* Fake calling sequence */

// TODO: how big to make workbuffer
#define TODO_WORKBUF_SIZE 16384

/**
 * PHASE 4: Hash the firmware body
 *
 * Any failures in this phase should trigger a reboot so we can try
 * the other firmware slot or go to recovery mode.
 */
int phase4_try_body(struct vb2_context *ctx)
{
	uint32_t expect_size;
	uint32_t size;
	uint8_t *ptr;
	int rv;

	/* Start the body hash */
	rv = vb2api_init_hash(ctx, VB2_HASH_TAG_FW_BODY, &expect_size);
	if (rv)
		return rv;

	/* Extend over the body */
	while (expect_size) {
		/* Read next body block */
		size = fake_read_next_body(&ptr);
		if (size <= 0)
			break;

		/* Hash it */
		rv = vb2api_extend_hash(ctx, ptr, size);
		if (rv)
			return rv;

		expect_size -= size;
	}

	/* Check the result */
	rv = vb2api_check_hash(ctx);
	if (rv)
		return rv;

	return VB2_SUCCESS;
}

/**
 * Save non-volatile and/or secure data if needed.
 */
void save_if_needed(struct vb2_context *ctx)
{

	if (ctx->flags & VB2_CONTEXT_NVDATA_CHANGED) {
		fake_save_nvdata(ctx->nvdata);
		ctx->flags &= ~VB2_CONTEXT_NVDATA_CHANGED;
	}

	if (ctx->flags & VB2_CONTEXT_SECDATA_CHANGED) {
		fake_save_secdata(ctx->secdata);
		ctx->flags &= ~VB2_CONTEXT_SECDATA_CHANGED;
	}
}

int sub_sequence(struct vb2_context *ctx)
{
	int rv;

	/* Hash the firmware body */
	rv = phase4_try_body(ctx);
	if (rv) {
		vb2api_fail(ctx, VBNV_RECOVERY_RO_INVALID_RW, rv);
		return fake_reboot();
	}

	// TODO: need to persist shared data across to the translation func
	// we'll add before kernel verification.
	// Do we want to pass the entire kernel key across, or just a SHA512
	// of it and reload it on the other side?

	return VB2_SUCCESS;
}

int calling_sequence(void)
{
	struct vb2_context ctx;
	uint8_t workbuf[TODO_WORKBUF_SIZE];
	int rv;

	/* Set up context */

	memset(&ctx, 0, sizeof(ctx));

	ctx.workbuf = workbuf;
	ctx.workbuf_size = sizeof(workbuf);

	fake_read_nvdata(ctx.nvdata);
	fake_read_secdata(ctx.secdata);

	if (fake_read_recovery_button())
		ctx.flags |= VB2_CONTEXT_FORCE_RECOVERY_MODE;

	if (fake_read_hw_dev_switch())
		ctx.flags |= VB2_CONTEXT_FORCE_DEVELOPER_MODE;

	/* Do early init */
	rv = vb2api_fw_phase1(&ctx);
	if (rv) {
		/* If we need recovery mode, leave firmware selection now */
		save_if_needed(&ctx);
		return fake_boot_to_recovery_mode();
	}

	/* Determine which firmware slot to boot */
	rv = vb2api_fw_phase2(&ctx);
	if (rv) {
		save_if_needed(&ctx);
		return fake_reboot();
	}

	/* Try that slot */
	rv = vb2api_fw_phase3(&ctx);
	if (rv) {
		save_if_needed(&ctx);
		return fake_reboot();
	}

	/* Save any changes to secure storage data then lock it */
	save_if_needed(&ctx);
	fake_lock_secdata();


	rv = sub_sequence(&ctx);

	save_if_needed(&ctx);

	return rv;
}
