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
#include "host_p11_21.h"

static CK_FUNCTION_LIST_PTR p11 = NULL;

enum vb2_hash_algorithm p11_mechanism_to_hash_alg(CK_MECHANISM_TYPE p11_mechanism)
{
	switch (p11_mechanism) {
	case CKM_SHA1_RSA_PKCS:
		return VB2_HASH_SHA1;
	case CKM_SHA256_RSA_PKCS:
		return VB2_HASH_SHA256;
	case CKM_SHA512_RSA_PKCS:
		return VB2_HASH_SHA512;
	case CKM_SHA224_RSA_PKCS:
		return VB2_HASH_SHA224;
	case CKM_SHA384_RSA_PKCS:
		return VB2_HASH_SHA384;
	}
	return VB2_HASH_INVALID;
}

/*static CK_ULONG get_p11_signature_size(struct pkcs11_key *p11_key) {*/
/*CK_RV result;*/
/*result = p11->C_SignInit(p11_key->session, &p11_key->mechanism, p11_key->handle);*/
/*if (result != CKR_OK) {*/
/*fprintf(stderr, "Failed to sign init\n");*/
/*return NULL;*/
/*}*/
/*CK_ULONG sig_size = 0;*/
/*uint8_t tmp_data[] = {1, 2, 3};*/
/*result = p11->C_Sign(p11_key->session, tmp_data, sizeof(tmp_data), NULL, &sig_size);*/
/*if (result != CKR_OK) {*/
/*fprintf(stderr, "Failed to get signature size\n");*/
/*return NULL;*/
/*}*/
/*uint8_t *tmp_sig = malloc(sig_size);*/
/*result = p11->C_Sign(p11_key->session, tmp_data, sizeof(tmp_data), tmp_sig, &sig_size);*/
/*free(tmp_sig);*/
/*if (result != CKR_OK) {*/
/*fprintf(stderr, "Failed to sign\n");*/
/*return NULL;*/
/*}*/
/*return sig_size;*/
/*}*/

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

static void *pkcs11_load(const char *mspec, CK_FUNCTION_LIST_PTR_PTR funcs)
{
	void *mod;
	CK_RV rv, (*c_get_function_list)(CK_FUNCTION_LIST_PTR_PTR);
	if (mspec == NULL)
		return NULL;

	fprintf(stderr, "loading module: %s\n", mspec);

	mod = dlopen(mspec, RTLD_LAZY);
	if (mod == NULL) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return NULL;
	}

	/* Get the list of function pointers */
	c_get_function_list =
		(CK_RV(*)(CK_FUNCTION_LIST_PTR_PTR))dlsym(mod, "C_GetFunctionList");
	if (!c_get_function_list)
		goto failed;
	rv = c_get_function_list(funcs);
	if (rv == CKR_OK)
		return mod;
	fprintf(stderr, "C_GetFunctionList failed 0x%lx", rv);
failed:
	dlclose(mod);
	return NULL;
}

static bool pkcs11_find(CK_SESSION_HANDLE session, CK_ATTRIBUTE attributes[],
			CK_ULONG num_attributes, CK_OBJECT_HANDLE *object)
{
	CK_RV result = p11->C_FindObjectsInit(session, attributes, num_attributes);
	if (result != CKR_OK)
		return false;

	CK_ULONG object_count = 1;
	result = p11->C_FindObjects(session, object, 1, &object_count);
	if (result != CKR_OK || object_count == 0)
		return false;

	result = p11->C_FindObjectsFinal(session);
	if (result != CKR_OK)
		return false;

	return true;
}

void pkcs11_init(void)
{
	static void *pkcs11_mod = NULL;
	if (pkcs11_mod != NULL) {
		fprintf(stderr, "Pkcs11 module is already loaded\n");
		return;
	}

	char *pkcs11_lib = getenv("PKCS11_MODULE_PATH");
	if (pkcs11_lib == NULL) {
		fprintf(stderr, "PKCS11_MODULE_PATH is not set\n");
		exit(1);
	}

	fprintf(stderr, "Loading pkcs11 module\n");
	pkcs11_mod = pkcs11_load(pkcs11_lib, &p11);
	if (pkcs11_mod == NULL) {
		fprintf(stderr, "Failed to load pkcs11\n");
		exit(1);
	}
	CK_RV result = p11->C_Initialize(NULL);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to initialize\n");
		exit(1);
	}
}

bool pkcs11_get_key(const struct pkcs11_key_info *key_info, struct pkcs11_key *p11_key)
{
	pkcs11_init();
	CK_RV result;
	result = p11->C_OpenSession(key_info->slot_id, CKF_SERIAL_SESSION | CKF_RW_SESSION,
				    NULL, // Ignore callbacks.
				    NULL, // Ignore callbacks.
				    &p11_key->session);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to open session\n");
		return false;
	}

	CK_OBJECT_CLASS class_value = CKO_PRIVATE_KEY;
	CK_ATTRIBUTE attributes[] = {
		{CKA_CLASS, &class_value, sizeof(class_value)},
		{CKA_LABEL, key_info->label, strlen(key_info->label)},
	};
	if (!pkcs11_find(p11_key->session, attributes, 2, &p11_key->handle)) {
		fprintf(stderr, "Failed to pkcs11 find\n");
		return false;
	}

	CK_ATTRIBUTE modulus_attr = {CKA_MODULUS, NULL, 0};
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &modulus_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get attribute length\n");
		return false;
	}
	modulus_attr.pValue = malloc(modulus_attr.ulValueLen);
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &modulus_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get attribute value\n");
		free(modulus_attr.pValue);
		return false;
	}
	p11_key->signature_size = modulus_attr.ulValueLen;
	fprintf(stderr, "signature size: %u\n", p11_key->signature_size);

	CK_ATTRIBUTE mechanism_attr = {CKA_ALLOWED_MECHANISMS, NULL, 0};
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &mechanism_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get attribute length\n");
		return false;
	}
	mechanism_attr.pValue = malloc(mechanism_attr.ulValueLen);
	if (p11->C_GetAttributeValue(p11_key->session, p11_key->handle, &mechanism_attr, 1) !=
	    CKR_OK) {
		fprintf(stderr, "Failed to get attribute value\n");
		free(mechanism_attr.pValue);
		return false;
	}
	CK_MECHANISM_TYPE mechanism_type = 0;
	for (int i = 0; i < mechanism_attr.ulValueLen / sizeof(CK_MECHANISM_TYPE); ++i) {
		CK_MECHANISM_TYPE *mechanisms = mechanism_attr.pValue;
		fprintf(stderr, "mechanisms[%d]: 0x%lx\n", i, mechanisms[i]);
		if (p11_mechanism_to_hash_alg(mechanisms[i]) != VB2_HASH_INVALID) {
			mechanism_type = mechanisms[i];
			break;
		}
	}
	fprintf(stderr, "mechanism_type: 0x%lx\n", mechanism_type);
	p11_key->mechanism.mechanism = mechanism_type;
	p11_key->mechanism.pParameter = NULL;
	p11_key->mechanism.ulParameterLen = 0;
	return true;
}

bool pkcs11_sign(struct pkcs11_key *p11_key, const uint8_t *data, int data_size, uint8_t *sig,
		 CK_ULONG sig_size)
{
	pkcs11_init();
	CK_RV result;
	result = p11->C_SignInit(p11_key->session, &p11_key->mechanism, p11_key->handle);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to sign init\n");
		return NULL;
	}
	result = p11->C_Sign(p11_key->session, (unsigned char *)data, data_size, NULL,
			     &sig_size);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to get signature size\n");
		return NULL;
	}
	result =
		p11->C_Sign(p11_key->session, (unsigned char *)data, data_size, sig, &sig_size);
	if (result != CKR_OK) {
		fprintf(stderr, "Failed to sign\n");
		return NULL;
	}
	return sig;
}
