/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_HOST_P11_H_
#define VBOOT_REFERENCE_HOST_P11_H_

#include <nss/pkcs11.h>

#include "2id.h"
#include "2return_codes.h"
#include "2struct.h"

struct pkcs11_key_info {
	char *label;
	CK_SLOT_ID slot_id;
};

struct pkcs11_key {
	CK_OBJECT_HANDLE handle;
	CK_SESSION_HANDLE session;
	uint32_t signature_size;
	CK_MECHANISM mechanism;
};

enum vb2_signature_algorithm sig_size_to_sig_alg(uint32_t sig_size);

void pkcs11_init(void);

bool pkcs11_get_key(const struct pkcs11_key_info *key_info, struct pkcs11_key *p11_key);

bool pkcs11_sign(struct pkcs11_key *p11_key, const uint8_t *data, int data_size, uint8_t *sig,
		 CK_ULONG sig_size);

#endif /* VBOOT_REFERENCE_HOST_P11_H_ */
