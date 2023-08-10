/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#include <nss/pkcs11.h>

#include "2common.h"
#include "host_p11.h"

// We only maintain one global p11 module at a time.
static CK_FUNCTION_LIST_PTR p11 = NULL;

static void *pkcs11_load(const char *mspec, CK_FUNCTION_LIST_PTR_PTR funcs)
{
	void *mod;
	CK_RV rv;
	CK_RV (*c_get_function_list)(CK_FUNCTION_LIST_PTR_PTR p11);

	if (mspec == NULL)
		return NULL;

	mod = dlopen(mspec, RTLD_LAZY);
	if (mod == NULL) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return NULL;
	}

	/* Get the list of function pointers */
	c_get_function_list =
		(CK_RV(*)(CK_FUNCTION_LIST_PTR_PTR))dlsym(mod, "C_GetFunctionList");
	if (!c_get_function_list)
		goto err;
	rv = c_get_function_list(funcs);
	if (rv == CKR_OK)
		return mod;
	fprintf(stderr, "C_GetFunctionList failed 0x%lx", rv);
err:
	dlclose(mod);
	return NULL;
}

static vb2_error_t pkcs11_find(CK_SESSION_HANDLE session, CK_ATTRIBUTE attributes[],
			       CK_ULONG num_attributes, CK_OBJECT_HANDLE *object)
{
	CK_RV result = p11->C_FindObjectsInit(session, attributes, num_attributes);
	if (result != CKR_OK)
		return VB2_ERROR_UNKNOWN;

	CK_ULONG object_count = 1;
	result = p11->C_FindObjects(session, object, 1, &object_count);
	if (result != CKR_OK || object_count == 0)
		return VB2_ERROR_UNKNOWN;

	result = p11->C_FindObjectsFinal(session);
	if (result != CKR_OK)
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

static bool valid_pkcs11_mechanism(CK_MECHANISM_TYPE p11_mechanism)
{
	switch (p11_mechanism) {
	case CKM_SHA1_RSA_PKCS:
	case CKM_SHA256_RSA_PKCS:
	case CKM_SHA512_RSA_PKCS:
	case CKM_SHA224_RSA_PKCS:
	case CKM_SHA384_RSA_PKCS:
		return true;
	}
	return false;
}

enum vb2_signature_algorithm sig_size_to_sig_alg(uint32_t sig_size)
{
	switch (sig_size) {
	case 1024 / 8:
		return VB2_SIG_RSA1024;
	case 2048 / 8:
		return VB2_SIG_RSA2048;
	case 4096 / 8:
		return VB2_SIG_RSA4096;
	case 8192 / 8:
		return VB2_SIG_RSA8192;
	}
	return VB2_SIG_INVALID;
}

vb2_error_t pkcs11_init(const char *pkcs11_lib)
{
	static void *pkcs11_mod = NULL;
	if (pkcs11_mod != NULL) {
		fprintf(stderr, "Pkcs11 module is already loaded\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (pkcs11_lib == NULL) {
		fprintf(stderr, "Missing the path of pkcs11 library\n");
		return VB2_ERROR_UNKNOWN;
	}

	pkcs11_mod = pkcs11_load(pkcs11_lib, &p11);
	if (pkcs11_mod == NULL) {
		fprintf(stderr, "Failed to load pkcs11\n");
		return VB2_ERROR_UNKNOWN;
	}
	CK_RV result = p11->C_Initialize(NULL);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to C_Initialize\n");
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

vb2_error_t pkcs11_get_key(int slot_id, char *label, struct pkcs11_key *p11_key)
{
	if (!p11) {
		fprintf(stderr, "pkcs11 is not loaded\n");
		return VB2_ERROR_UNKNOWN;
	}

	CK_RV result = p11->C_OpenSession(slot_id, CKF_SERIAL_SESSION | CKF_RW_SESSION, NULL,
					  NULL, &p11_key->session);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to open session\n");
		return VB2_ERROR_UNKNOWN;
	}

	/* Find the private key */
	CK_OBJECT_CLASS class_value = CKO_PRIVATE_KEY;
	CK_ATTRIBUTE attributes[] = {
		{CKA_CLASS, &class_value, sizeof(class_value)},
		{CKA_LABEL, label, strlen(label)},
	};
	if (pkcs11_find(p11_key->session, attributes, 2, &p11_key->handle) != VB2_SUCCESS) {
		fprintf(stderr, "Failed to pkcs11 find\n");
		return VB2_ERROR_UNKNOWN;
	}

	/* Get the signature size */
	CK_ATTRIBUTE modulus_attr = {CKA_MODULUS, NULL, 0};
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &modulus_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get modulus attribute length\n");
		return VB2_ERROR_UNKNOWN;
	}
	p11_key->signature_size = modulus_attr.ulValueLen;

	/* Find the suitable mechanism to sign */
	/* For PKCS#11 modules that support CKA_ALLOWED_MECHANISMS, we'll use the attribute
	 * to determine the correct mechanism to use. However, not all PKCS#11 modules
	 * support CKA_ALLOWED_MECHANISMS. In the event that we need to support such a
	 * module, we'll then need to determine the the mechanism to use from the key type
	 * and key size. That probably involves assuming we'll use PKCS#1 v1.5 padding for
	 * RSA. */
	CK_ATTRIBUTE mechanism_attr = {CKA_ALLOWED_MECHANISMS, NULL, 0};
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &mechanism_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get mechanisum attribute length\n");
		return VB2_ERROR_UNKNOWN;
	}
	mechanism_attr.pValue = malloc(mechanism_attr.ulValueLen);
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &mechanism_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get mechanisum attribute value\n");
		free(mechanism_attr.pValue);
		return VB2_ERROR_UNKNOWN;
	}
	CK_MECHANISM_TYPE *mechanisms = mechanism_attr.pValue;
	uint32_t mechanism_count = mechanism_attr.ulValueLen / sizeof(CK_MECHANISM_TYPE);
	for (int i = 0; i < mechanism_count; ++i) {
		if (valid_pkcs11_mechanism(mechanisms[i])) {
			p11_key->mechanism.mechanism = mechanisms[i];
			break;
		}
	}
	p11_key->mechanism.pParameter = NULL;
	p11_key->mechanism.ulParameterLen = 0;
	free(mechanism_attr.pValue);
	return VB2_SUCCESS;
}

vb2_error_t pkcs11_sign(struct pkcs11_key *p11_key, const uint8_t *data, int data_size,
			uint8_t *sig, CK_ULONG sig_size)
{
	if (!p11) {
		fprintf(stderr, "pkcs11 is not loaded\n");
		return VB2_ERROR_UNKNOWN;
	}
	CK_RV result = p11->C_SignInit(p11_key->session, &p11_key->mechanism, p11_key->handle);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to sign init\n");
		return VB2_ERROR_UNKNOWN;
	}
	result =
		p11->C_Sign(p11_key->session, (unsigned char *)data, data_size, sig, &sig_size);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to sign\n");
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

void pkcs11_free_key(struct pkcs11_key *p11_key)
{
	if (!p11) {
		fprintf(stderr, "pkcs11 is not loaded\n");
		return;
	}
	CK_RV result = p11->C_CloseSession(p11_key->session);
	if (result != CKR_OK)
		fprintf(stderr, "Failed to close session\n");
	free(p11_key);
}
