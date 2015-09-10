/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host functions for boot descriptor blocks
 */

#include "bdb.h"

/***
Expected calling sequence:

Load and check just the header
bdb_check_header(buf, size);

Load and verify the entire BDB
bdb_verify(buf, size, bdb_key_hash, dev_mode_flag);

	bdb_check_header() again - paranoia against bad storage devices

	bdb_check_key() on BDB key
	bdb_sha256() on BDB key
	Compare with appropriate root key hash
	If dev_mode_flag(), mismatch is not fatal

	bdb_check_sig() on BDB header sig
	bdb_sha256() on OEM area 1, RW subkey
	bdb_rsa_verify() on digest using BDB key

	bdb_check_key() on RW subkey

	bdb_check_data() on RW data
	bdb_check_sig() on data sig
	bdb_sha256() on data, OEM area 1, hashes
	bdb_rsa_verify() on digest using RW subkey

Check RW subkey version.  If normal boot from primary BDB, roll forward
Check data version.  If normal boot from primary BDB, roll forward
***/

/*****************************************************************************/

/* Codes for functions returning numeric error codes */
enum bdb_return_code {
	/* Success */
	BDB_SUCCESS = 0,

	/* BDB key did not match hash, but other than that the BDB was
	 * fully verified. */
	BDB_GOOD_OTHER_THAN_KEY = 1,

	/* Other errors */
	BDB_ERROR_UNKNOWN = 100,
};

/**
 * Sanity-check BDB structures.
 *
 * This checks for known version numbers, magic numbers, algorithms, etc. and
 * ensures the sizes are consistent with those parameters.
 *
 * @param p		Pointer to structure to check
 * @param size		Size of structure buffer
 * @return 0 if success, non-zero error code if error.
 */
int bdb_check_header(const struct bdb_header *p, size_t size);
int bdb_check_key(const struct bdb_key *p, size_t size);
int bdb_check_sig(const struct bdb_sig *p, size_t size);
int bdb_check_data(const struct bdb_data *p, size_t size);

/**
 * Verify the entire BDB
 *
 * @param buf			Data to hash
 * @param size			Size of data in bytes
 * @param bdb_key_digest	Pointer to expected digest for BDB key.
 *				Must be BDB_SHA256_DIGEST_SIZE bytes long.
 *
 * @return 0 if success, non-zero error code if error.  Note that error code
 * BDB_GOOD_OTHER_THAN_KEY may still indicate an acceptable BDB if the
 * Boot Verified fuse has not been set, or in developer mode.
 */
int bdb_verify(const void *buf, size_t size, const uint8_t *bdb_key_digest);

/**
 * Functions to extract things from a verified BDB buffer.
 *
 * Do not call these externally until after bdb_verify()!  These methods
 * assume data structures have already been verified.
 *
 * @param buf		Pointer to BDB buffer
 * @param size		Size of BDB buffer in bytes
 * @param type		Data type, for bdb_get_hash()
 * @return A pointer to the requested data, or NULL if error / not present.
 */
const struct bdb_header *bdb_get_header(const void *buf, size_t size);
const struct bdb_key *bdb_get_bdb_key(const void *buf, size_t size);
const struct bdb_sig *bdb_get_header_sig(const void *buf, size_t size);
const void *bdb_get_oem_area_0(const void *buf, size_t size);
const struct bdb_key *bdb_get_subkey(const void *buf, size_t size);
const struct bdb_data *bdb_get_data(const void *buf, size_t size);
const struct bdb_data *bdb_get_data_sig(const void *buf, size_t size);
const void *bdb_get_oem_area_1(const void *buf, size_t size);
const struct bdb_hash *bdb_get_hash(const void *buf, size_t size,
				    enum bdb_data_type type);

/*****************************************************************************/
/* Functions probably provided by the caller */

/**
 * Calculate a SHA-256 digest of a buffer.
 *
 * @param digest	Pointer to the digest buffer.  Must be
 *			BDB_SHA256_DIGEST_SIZE bytes long.
 * @param buf		Data to hash
 * @param size		Size of data in bytes
 * @return 0 if success, non-zero error code if error.
 */
__attribute__((weak))
int bdb_sha256(void *digest, const void *buf, size_t size);

/**
 * Verify a RSA-4096 signed digest
 *
 * @param key_data	Key data to use (BDB_RSA4096_KEY_DATA_SIZE bytes)
 * @param sig_data	Signature to verify (BDB_RSA4096_SIG_SIZE bytes)
 * @param digest	Digest of signed data (BDB_SHA256_DIGEST bytes)
 * @return 0 if success, non-zero error code if error.
 */
__attribute__((weak))
int bdb_rsa4096_verify(const uint8_t *key_data,
		       const uint8_t *sig,
		       const uint8_t *digest);
