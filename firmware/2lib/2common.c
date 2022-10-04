/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 * (Firmware portion)
 */

#include "2common.h"
#include "2rsa.h"

test_mockable
vb2_error_t vb2_safe_memcmp(const void *s1, const void *s2, size_t size)
{
	const unsigned char *us1 = s1;
	const unsigned char *us2 = s2;
	int result = 0;

	if (0 == size)
		return 0;

	/*
	 * Code snippet without data-dependent branch due to Nate Lawson
	 * (nate@root.org) of Root Labs.
	 */
	while (size--)
		result |= *us1++ ^ *us2++;

	return result != 0;
}

vb2_error_t vb2_align(uint8_t **ptr, uint32_t *size, uint32_t align,
		      uint32_t want_size)
{
	uintptr_t p = (uintptr_t)*ptr;
	uintptr_t offs = p & (align - 1);

	if (offs) {
		offs = align - offs;

		if (*size < offs)
			return VB2_ERROR_ALIGN_BIGGER_THAN_SIZE;

		*ptr += offs;
		*size -= offs;
	}

	if (*size < want_size)
		return VB2_ERROR_ALIGN_SIZE;

	return VB2_SUCCESS;
}

void vb2_workbuf_init(struct vb2_workbuf *wb, uint8_t *buf, uint32_t size)
{
	wb->buf = buf;
	wb->size = size;

	/* Align the buffer so allocations will be aligned */
	if (vb2_align(&wb->buf, &wb->size, VB2_WORKBUF_ALIGN, 0))
		wb->size = 0;
}

void *vb2_workbuf_alloc(struct vb2_workbuf *wb, uint32_t size)
{
	uint8_t *ptr = wb->buf;

	/* Round up size to work buffer alignment */
	size = vb2_wb_round_up(size);

	if (size > wb->size)
		return NULL;

	wb->buf += size;
	wb->size -= size;

	return ptr;
}

void *vb2_workbuf_realloc(struct vb2_workbuf *wb, uint32_t oldsize,
			  uint32_t newsize)
{
	/*
	 * Just free and allocate to update the size.  No need to move/copy
	 * memory, since the new pointer is guaranteed to be the same as the
	 * old one.  The new allocation can fail, if the new size is too big.
	 */
	vb2_workbuf_free(wb, oldsize);
	return vb2_workbuf_alloc(wb, newsize);
}

void vb2_workbuf_free(struct vb2_workbuf *wb, uint32_t size)
{
	/* Round up size to work buffer alignment */
	size = vb2_wb_round_up(size);

	wb->buf -= size;
	wb->size += size;
}

ptrdiff_t vb2_offset_of(const void *base, const void *ptr)
{
	return (uintptr_t)ptr - (uintptr_t)base;
}

void *vb2_member_of(void *parent, ptrdiff_t offset)
{
	/* TODO(kitching): vb2_assert(parent && offset) */
	return parent + offset;
}

vb2_error_t vb2_verify_member_inside(const void *parent, size_t parent_size,
				     const void *member, size_t member_size,
				     ptrdiff_t member_data_offset,
				     size_t member_data_size)
{
	const uintptr_t parent_end = (uintptr_t)parent + parent_size;
	const ptrdiff_t member_offs = vb2_offset_of(parent, member);
	const ptrdiff_t member_end_offs = member_offs + member_size;
	const ptrdiff_t data_offs = member_offs + member_data_offset;
	const ptrdiff_t data_end_offs = data_offs + member_data_size;

	/* Make sure parent doesn't wrap */
	if (parent_end < (uintptr_t)parent)
		return VB2_ERROR_INSIDE_PARENT_WRAPS;

	/*
	 * Make sure the member is fully contained in the parent and doesn't
	 * wrap.  Use >, not >=, since member_size = 0 is possible.
	 */
	if (member_end_offs < member_offs)
		return VB2_ERROR_INSIDE_MEMBER_WRAPS;
	if (member_offs < 0 || member_offs > parent_size ||
	    member_end_offs > parent_size)
		return VB2_ERROR_INSIDE_MEMBER_OUTSIDE;

	/* Make sure the member data is after the member */
	if (member_data_size > 0 && data_offs < member_end_offs)
		return VB2_ERROR_INSIDE_DATA_OVERLAP;

	/* Make sure parent fully contains member data, if any */
	if (data_end_offs < data_offs)
		return VB2_ERROR_INSIDE_DATA_WRAPS;
	if (data_offs < 0 || data_offs > parent_size ||
	    data_end_offs > parent_size)
		return VB2_ERROR_INSIDE_DATA_OUTSIDE;

	return VB2_SUCCESS;
}

test_mockable
vb2_error_t vb2_verify_digest(const struct vb2_public_key *key,
			      struct vb2_signature *sig, const uint8_t *digest,
			      const struct vb2_workbuf *wb)
{
	/* A signature is destroyed in the process of being verified. */
	uint8_t *sig_data = vb2_signature_data_mutable(sig);

	if (sig->sig_size != vb2_rsa_sig_size(key->sig_alg)) {
		VB2_DEBUG("Wrong data signature size for algorithm, "
			  "sig_size=%d, expected %d for algorithm %d.\n",
			  sig->sig_size, vb2_rsa_sig_size(key->sig_alg),
			  key->sig_alg);
		return VB2_ERROR_VDATA_SIG_SIZE;
	}

	if (key->allow_hwcrypto) {
		vb2_error_t rv =
			vb2ex_hwcrypto_rsa_verify_digest(key, sig_data, digest);

		if (rv != VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED) {
			VB2_DEBUG("Using HW RSA engine for sig_alg %d %s\n",
					key->sig_alg,
					rv ? "failed" : "succeeded");
			return rv;
		}

		VB2_DEBUG("HW RSA for sig_alg %d not supported, using SW\n",
			  key->sig_alg);
	} else {
		VB2_DEBUG("HW RSA forbidden, using SW\n");
	}

	return vb2_rsa_verify_digest(key, sig_data, digest, wb);
}

test_mockable
vb2_error_t vb2_verify_data(const uint8_t *data, uint32_t size,
			    struct vb2_signature *sig,
			    const struct vb2_public_key *key,
			    const struct vb2_workbuf *wb)
{
	struct vb2_hash hash;

	if (sig->data_size > size) {
		VB2_DEBUG("Data buffer smaller than length of signed data.\n");
		return VB2_ERROR_VDATA_NOT_ENOUGH_DATA;
	}

	VB2_TRY(vb2_hash_calculate(key->allow_hwcrypto, data, sig->data_size,
				   key->hash_alg, &hash));

	return vb2_verify_digest(key, sig, hash.raw, wb);
}

void print_debug_info(struct vb2_context *ctx)
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
