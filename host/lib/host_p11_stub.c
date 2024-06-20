/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_p11.h"

vb2_error_t pkcs11_init(const char *pkcs11_lib)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return VB2_ERROR_UNKNOWN;
}

vb2_error_t pkcs11_get_key(int slot_id, char *label, struct pkcs11_key **p11_key)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return VB2_ERROR_UNKNOWN;
}

enum vb2_hash_algorithm pkcs11_get_hash_alg(struct pkcs11_key *p11_key)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return VB2_HASH_INVALID;
}

enum vb2_signature_algorithm pkcs11_get_sig_alg(struct pkcs11_key *p11_key)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return VB2_SIG_INVALID;
}

uint8_t *pkcs11_get_modulus(struct pkcs11_key *p11_key, uint32_t *sizeptr)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return NULL;
}

vb2_error_t pkcs11_sign(struct pkcs11_key *p11_key, enum vb2_hash_algorithm hash_alg,
			const uint8_t *data, int data_size, uint8_t *sig, uint32_t sig_size)
{
	VB2_DEBUG("%s() called, but compiled without PKCS#11 support (libnss).\n", __func__);
	return VB2_ERROR_UNKNOWN;
}

void pkcs11_free_key(struct pkcs11_key *p11_key)
{
}
