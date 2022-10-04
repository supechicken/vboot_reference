/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Secure storage APIs
 */

#include "2common.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"
#include "2sysincludes.h"

static void print_debug_info(struct vb2_context *ctx)
{

	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;
	struct vb2_secdata_fwmp *sec_fwmp =
		(struct vb2_secdata_fwmp *)ctx->secdata_fwmp;

	VB2_DEBUG("\n**************************************BEGIN DEBUG**********************************************\n");
	// CTX
	VB2_DEBUG("VB2_Context{\n\tFlags=0x%" PRIx64 "\n\tboot_mode=0x%02x\n}\n\n",ctx->flags,ctx->boot_mode);

	// Firmware
	VB2_DEBUG("\nvb2_secdata_firmware{\n\tstruct_version=0x%02x\n\tflags=0x%02x\n\tfw_version=0x%08x\n\treserved[0]=0x%02x", sec->struct_version,sec->flags,sec->fw_versions,sec->reserved[0]);
	VB2_DEBUG("\n\treserved[1]=0x%02x\n\treserved[2]=0x%02x\n\tcrc8=0x%02x\n}\n",sec->reserved[1],sec->reserved[2],sec->crc8 );
	unsigned int firmware_struct_size = sizeof(struct vb2_secdata_firmware);
	VB2_DEBUG("Size of struct = 0x%08x\nSize of Reserved Space = 0x%08x\n\n", firmware_struct_size, VB2_SECDATA_FIRMWARE_SIZE);
	for(int i = firmware_struct_size;i<VB2_SECDATA_FIRMWARE_SIZE;i++)
	{
		VB2_DEBUG("Padding[%d]=0x%02x\n", i, ctx->secdata_firmware[i]);
	}
	bool unknown_version = false;
	// Kernel
	VB2_DEBUG("\n\nKernel Struct Version 0x%02x\n", ctx->secdata_kernel[0]);
	unsigned int kernel_struct_size = 0;
	if(ctx->secdata_kernel[0] == 0x2)
	{
		// V0
		struct vb2_secdata_kernel_v0 *sec_kernel =
			(struct vb2_secdata_kernel_v0 *)ctx->secdata_kernel;
		VB2_DEBUG("\nvb2_secdata_kernel_v0{\n\tstruct_version=0x%02x\n\tuid=0x%08x\n\tkernel_versions=0x%08x\n", sec_kernel->struct_version, sec_kernel->uid, sec_kernel->kernel_versions);
		VB2_DEBUG("\n\treserved[0]=0x%02x\n\treserved[1]=0x%02x\n\treserved[2]=0x%02x\n\tcrc8=0x%02x\n}\n\n",sec_kernel->reserved[0],sec_kernel->reserved[1],sec_kernel->reserved[2],sec_kernel->crc8 );
		kernel_struct_size = sizeof(struct vb2_secdata_kernel_v0);
	}
	else if(ctx->secdata_kernel[0] == 0x10)
	{
		// V1
		struct vb2_secdata_kernel_v1 *sec_kernel =
			(struct vb2_secdata_kernel_v1 *)ctx->secdata_kernel;
		VB2_DEBUG("\nvb2_secdata_kernel_v1{\n\tstruct_version=0x%02x\n\tstruct_size=0x%02x\n\tcrc8=0x%02x\n\tflags=0x%02x\n\tkernel_versions=0x%08x\n", sec_kernel->struct_version, sec_kernel->struct_size, sec_kernel->crc8, sec_kernel->flags,sec_kernel->kernel_versions);
		for(int i =0; i < VB2_SHA256_DIGEST_SIZE;i++)
		{
			VB2_DEBUG("\n\tec_hash[%d]=0x%02x", i, sec_kernel->ec_hash[i]);
		}
		VB2_DEBUG("\n}\n\n");
		kernel_struct_size = sizeof(struct vb2_secdata_kernel_v1);
	}
	else
	{
		// Unknown
		unknown_version = true;
		VB2_DEBUG("\nvb2_secdata_kernel_unknown{\n");
		for(int i =0;i<VB2_SECDATA_KERNEL_MAX_SIZE;i++)
		{
			VB2_DEBUG("\n\tData[%d]=0x%02x\n", i, ctx->secdata_kernel[i]);
		}
		VB2_DEBUG("\n}\n\n");
	}
	if(!unknown_version)
	{
		VB2_DEBUG("Size of struct = 0x%08x\nSize of Reserved Space = 0x%08x\n\n", kernel_struct_size, VB2_SECDATA_KERNEL_MAX_SIZE);
		for(int i =kernel_struct_size;i<VB2_SECDATA_KERNEL_MAX_SIZE;i++)
		{
			VB2_DEBUG("Padding[%d]=0x%02x\n", i, ctx->secdata_kernel[i]);
		}
	}

	// FWMP

	VB2_DEBUG("\nvb2_secdata_fwmp{\n\tcrc8=0x%02x\n\tstruct_size=0x%02x\n\tstruct_version=0x%02x\n\treserved=0x%02x\n\tflags=0x%08x", sec_fwmp->crc8, sec_fwmp->struct_size, sec_fwmp->struct_version, sec_fwmp->reserved0, sec_fwmp->flags);
	for(int i =0; i < VB2_SECDATA_FWMP_HASH_SIZE;i++)
	{
		VB2_DEBUG("\n\tdev_key_hash[%d]=0x%02x", i, sec_fwmp->dev_key_hash[i]);
	}
	VB2_DEBUG("\n}\n\n");
	unsigned int fwmp_struct_size = sizeof(struct vb2_secdata_fwmp);
	VB2_DEBUG("Size of struct = 0x%08x\nSize of Reserved Space = 0x%08x\n\n", fwmp_struct_size, VB2_SECDATA_FWMP_MAX_SIZE);
	for(int i = fwmp_struct_size;i<VB2_SECDATA_FWMP_MAX_SIZE;i++)
	{
		VB2_DEBUG("Padding[%d]=0x%02x\n", i, ctx->secdata_fwmp[i]);
	}

	VB2_DEBUG("\n**************************************END DEBUG**********************************************\n");
}

vb2_error_t vb2api_secdata_firmware_check(struct vb2_context *ctx)
{
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;

	print_debug_info(ctx);

	/* Verify CRC */
	if (sec->crc8 != vb2_crc8(sec, offsetof(struct vb2_secdata_firmware,
						crc8))) {
		VB2_DEBUG("secdata_firmware: bad CRC\n");
		VB2_DEBUG("Expected: 0x%02x - Got: 0x%02x\n", sec->crc8, vb2_crc8(sec, offsetof(struct vb2_secdata_firmware,crc8)));
		return VB2_ERROR_SECDATA_FIRMWARE_CRC;
	}

	/* Verify version */
	if (sec->struct_version < VB2_SECDATA_FIRMWARE_VERSION) {
		VB2_DEBUG("secdata_firmware: version incompatible\n");
		return VB2_ERROR_SECDATA_FIRMWARE_VERSION;
	}

	return VB2_SUCCESS;
}

uint32_t vb2api_secdata_firmware_create(struct vb2_context *ctx)
{
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;

	/* Clear the entire struct */
	memset(sec, 0, sizeof(*sec));

	/* Set to current version */
	sec->struct_version = VB2_SECDATA_FIRMWARE_VERSION;

	/* Calculate initial CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_firmware, crc8));

	/* Mark as changed */
	ctx->flags |= VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;

	return sizeof(*sec);
}

vb2_error_t vb2_secdata_firmware_init(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	VB2_TRY(vb2api_secdata_firmware_check(ctx));

	/* Set status flag */
	sd->status |= VB2_SD_STATUS_SECDATA_FIRMWARE_INIT;

	/* Read this now to make sure crossystem has it even in rec mode */
	sd->fw_version_secdata =
		vb2_secdata_firmware_get(ctx, VB2_SECDATA_FIRMWARE_VERSIONS);

	return VB2_SUCCESS;
}

uint32_t vb2_secdata_firmware_get(struct vb2_context *ctx,
				  enum vb2_secdata_firmware_param param)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;
	const char *msg;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FIRMWARE_INIT)) {
		msg = "get before init";
		goto fail;
	}

	switch (param) {
	case VB2_SECDATA_FIRMWARE_FLAGS:
		return sec->flags;

	case VB2_SECDATA_FIRMWARE_VERSIONS:
		return sec->fw_versions;

	default:
		msg = "invalid param";
	}

 fail:
	VB2_REC_OR_DIE(ctx, "%s\n", msg);
	return 0;
}

void vb2_secdata_firmware_set(struct vb2_context *ctx,
			      enum vb2_secdata_firmware_param param,
			      uint32_t value)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;
	const char *msg;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FIRMWARE_INIT)) {
		msg = "set before init";
		goto fail;
	}

	/* If not changing the value, just return early */
	if (value == vb2_secdata_firmware_get(ctx, param))
		return;

	switch (param) {
	case VB2_SECDATA_FIRMWARE_FLAGS:
		/* Make sure flags is in valid range */
		if (value > 0xff) {
			msg = "flags out of range";
			goto fail;
		}

		VB2_DEBUG("secdata_firmware flags updated from %#x to %#x\n",
			  sec->flags, value);
		sec->flags = value;
		break;

	case VB2_SECDATA_FIRMWARE_VERSIONS:
		VB2_DEBUG("secdata_firmware versions updated from "
			  "%#x to %#x\n",
			  sec->fw_versions, value);
		sec->fw_versions = value;
		break;

	default:
		msg = "invalid param";
		goto fail;
	}

	/* Regenerate CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_firmware, crc8));
	ctx->flags |= VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	return;

 fail:
	VB2_REC_OR_DIE(ctx, "%s\n", msg);
}
