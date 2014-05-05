/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 */

#ifndef VBOOT_REFERENCE_VBOOT_2COMMON_H_
#define VBOOT_REFERENCE_VBOOT_2COMMON_H_

#include "2struct.h"

struct vb2_public_key;

/* Return true if pointer is 32-bit aligned */
#define is_aligned_32(ptr) (!(((size_t)(ptr)) & 3))

/* Debug output */
#if defined(VBOOT_DEBUG) && !defined(VB2_DEBUG)
#include <stdio.h>
#define VB2_DEBUG(format, args...) printf(format, ## args)
#else
#define VB2_DEBUG(format, args...)
#endif

/**
 * Return offset of ptr from base.
 *
 * @param base		Base pointer
 * @param ptr		Pointer at some offset from base
 * @return The offset of ptr from base.
 */
uint32_t vb2_offset_of(const void *base, const void *ptr);

/*
 * Helper functions to get data pointed to by a public key or signature.
 */

const uint8_t *vb2_packed_key_data(const struct vb2_packed_key *key);
uint8_t *vb2_signature_data(struct vb2_signature *sig);

/**
 * Verify the data pointed to by a subfield is inside the parent data.
 *
 * The subfield has a header pointed to by member, and a separate data
 * field at an offset relative to the header.
 *
 * @param parent		Parent data
 * @param parent_size		Parent size in bytes
 * @param member		Subfield header
 * @param member_size		Size of subfield header in bytes
 * @param member_data_offset	Offset of member data from start of member
 * @param member_data_size	Size of member data in bytes
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_verify_member_inside(const void *parent, uint32_t parent_size,
			     const void *member, uint32_t member_size,
			     uint32_t member_data_offset,
			     uint32_t member_data_size);

/**
 * Verify a signature is fully contained in its parent data
 *
 * @param parent	Parent data
 * @param parent_size	Parent size in bytes
 * @param sig		Signature pointer
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_verify_signature_inside(const void *parent,
				uint32_t parent_size,
				const struct vb2_signature *sig);

/**
 * Verify a packed key is fully contained in its parent data
 *
 * @param parent	Parent data
 * @param parent_size	Parent size in bytes
 * @param key		Packed key pointer
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_verify_packed_key_inside(const void *parent,
				 uint32_t parent_size,
				 const struct vb2_packed_key *key);

/**
 * Unpack a RSA key for use in verification
 *
 * The elements of the unpacked key will point into the source buffer, so don't
 * free the source buffer until you're done with the key.
 *
 * @param rsa		Destintion for unpacked key
 * @param buf		Source buffer containing packed key
 * @param key		Size of buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_unpack_key(struct vb2_public_key *key,
		   const uint8_t *buf,
		   uint32_t size);

/* Size of work buffer sufficient for vb2_rsa_verifyData() worst case */
#define VB2_VERIFY_DATA_WORKBUF_BYTES \
	(VB2_VERIFY_DIGEST_WORKBUF_BYTES + SHA512_DIGEST_SIZE)

/**
 * Verify data matches signature.
 *
 * @param data		Data to verify
 * @param size		Size of data buffer.  Note that amount of data to
 *			actually validate is contained in sig->data_size.
 * @param sig		Signature of data (destroyed in process)
 * @param key		Key to use to validate signature
 * @param workbuf	Work buffer
 * @param workbuf_size	Size of work buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_verify_data(const uint8_t *data,
		    uint32_t size,
		    struct vb2_signature *sig,
		    const struct vb2_public_key *key,
		    uint8_t *workbuf,
		    uint32_t workbuf_size);

/* Size of work buffer sufficient for vb2_verify_keyblock() worst case */
#define VB2_KEY_BLOCK_VERIFY_WORKBUF_BYTES VB2_VERIFY_DATA_WORKBUF_BYTES

/**
 * Check the sanity of a key block using a public key.
 *
 * Header fields are also checked for sanity.  Does not verify key index or key
 * block flags.  Signature inside block is destroyed during check.
 *
 * @param block		Key block to verify
 * @param size		Size of key block buffer
 * @param key		Key to use to verify block
 * @param workbuf	Work buffer
 * @param workbuf_size	Size of work buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_verify_keyblock(struct vb2_keyblock *block,
			uint32_t size,
			const struct vb2_public_key *key,
			uint8_t *workbuf,
			uint32_t workbuf_size);

/* Size of work buffer sufficient for vb2_verify_fw_preamble() worst case */
#define VB2_VERIFY_FIRMWARE_PREAMBLE_WORKBUF_BYTES VB2_VERIFY_DATA_WORKBUF_BYTES

/**
 * Check the sanity of a firmware preamble using a public key.
 *
 * The signature in the preamble is destroyed during the check.
 *
 * @param preamble     	Preamble to verify
 * @param size		Size of preamble buffer
 * @param key		Key to use to verify preamble
 * @param workbuf	Work buffer
 * @param workbuf_size	Size of work buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_verify_fw_preamble(struct vb2_fw_preamble *preamble,
			      uint32_t size,
			      const struct vb2_public_key *key,
			      uint8_t *workbuf,
			      uint32_t workbuf_len);

#endif  /* VBOOT_REFERENCE_VBOOT_2COMMON_H_ */
