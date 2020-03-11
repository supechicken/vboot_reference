/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Hash and signature algorithm parsing helpers for host utilities.
 */

#include <stdlib.h>

#include "2rsa.h"
#include "2sha.h"
#include "host_common.h"
#include "vboot_host.h"

const struct vb2_text_vs_enum vb2_text_vs_sig[] = {
	{"RSA1024", VB2_SIG_RSA1024},
	{"RSA2048", VB2_SIG_RSA2048},
	{"RSA4096", VB2_SIG_RSA4096},
	{"RSA8192", VB2_SIG_RSA8192},
	{"RSA2048EXP3", VB2_SIG_RSA2048_EXP3},
	{"RSA3072EXP3", VB2_SIG_RSA3072_EXP3},
	{0, 0}
};

const struct vb2_text_vs_enum vb2_text_vs_hash[] = {
	{"SHA1",   VB2_HASH_SHA1},
	{"SHA256", VB2_HASH_SHA256},
	{"SHA512", VB2_HASH_SHA512},
	{0, 0}
};

const struct vb2_text_vs_enum vb2_text_vs_crypto[] = {
	{"RSA1024 SHA1",   VB2_ALG_RSA1024_SHA1},
	{"RSA1024 SHA256", VB2_ALG_RSA1024_SHA256},
	{"RSA1024 SHA512", VB2_ALG_RSA1024_SHA512},
	{"RSA2048 SHA1",   VB2_ALG_RSA2048_SHA1},
	{"RSA2048 SHA256", VB2_ALG_RSA2048_SHA256},
	{"RSA2048 SHA512", VB2_ALG_RSA2048_SHA512},
	{"RSA4096 SHA1",   VB2_ALG_RSA4096_SHA1},
	{"RSA4096 SHA256", VB2_ALG_RSA4096_SHA256},
	{"RSA4096 SHA512", VB2_ALG_RSA4096_SHA512},
	{"RSA8192 SHA1",   VB2_ALG_RSA8192_SHA1},
	{"RSA8192 SHA256", VB2_ALG_RSA8192_SHA256},
	{"RSA8192 SHA512", VB2_ALG_RSA8192_SHA512},
	{"RSA2048 EXP3 SHA1",   VB2_ALG_RSA2048_EXP3_SHA1},
	{"RSA2048 EXP3 SHA256", VB2_ALG_RSA2048_EXP3_SHA256},
	{"RSA2048 EXP3 SHA512", VB2_ALG_RSA2048_EXP3_SHA512},
	{"RSA3072 EXP3 SHA1",   VB2_ALG_RSA3072_EXP3_SHA1},
	{"RSA3072 EXP3 SHA256", VB2_ALG_RSA3072_EXP3_SHA256},
	{"RSA3072 EXP3 SHA512", VB2_ALG_RSA3072_EXP3_SHA512},
	{0, 0}
};

const struct vb2_text_vs_enum vb2_file_vs_crypto[] = {
	{"rsa1024", VB2_ALG_RSA1024_SHA1},
	{"rsa1024", VB2_ALG_RSA1024_SHA256},
	{"rsa1024", VB2_ALG_RSA1024_SHA512},
	{"rsa2048", VB2_ALG_RSA2048_SHA1},
	{"rsa2048", VB2_ALG_RSA2048_SHA256},
	{"rsa2048", VB2_ALG_RSA2048_SHA512},
	{"rsa4096", VB2_ALG_RSA4096_SHA1},
	{"rsa4096", VB2_ALG_RSA4096_SHA256},
	{"rsa4096", VB2_ALG_RSA4096_SHA512},
	{"rsa8192", VB2_ALG_RSA8192_SHA1},
	{"rsa8192", VB2_ALG_RSA8192_SHA256},
	{"rsa8192", VB2_ALG_RSA8192_SHA512},
	{"rsa2048_exp3", VB2_ALG_RSA2048_EXP3_SHA1},
	{"rsa2048_exp3", VB2_ALG_RSA2048_EXP3_SHA256},
	{"rsa2048_exp3", VB2_ALG_RSA2048_EXP3_SHA512},
	{"rsa3072_exp3", VB2_ALG_RSA3072_EXP3_SHA1},
	{"rsa3072_exp3", VB2_ALG_RSA3072_EXP3_SHA256},
	{"rsa3072_exp3", VB2_ALG_RSA3072_EXP3_SHA512},
	{0, 0}
};

static const struct vb2_text_vs_enum *vb2_lookup_by_num(
	const struct vb2_text_vs_enum *table,
	const unsigned int num)
{
	for (; table->name; table++)
		if (table->num == num)
			return table;
	return 0;
}

static const struct vb2_text_vs_enum *vb2_lookup_by_name(
	const struct vb2_text_vs_enum *table,
	const char *name)
{
	for (; table->name; table++)
		if (!strcasecmp(table->name, name))
			return table;
	return 0;
}

const char *vb2_get_sig_algorithm_name(enum vb2_signature_algorithm sig_alg)
{
	const struct vb2_text_vs_enum *entry =
			vb2_lookup_by_num(vb2_text_vs_sig, sig_alg);

	return entry ? entry->name : VB2_INVALID_ALG_NAME;
}

const char *vb2_get_crypto_algorithm_name(enum vb2_crypto_algorithm alg)
{
	const struct vb2_text_vs_enum *entry =
			vb2_lookup_by_num(vb2_text_vs_crypto, alg);

	return entry ? entry->name : VB2_INVALID_ALG_NAME;
}

const char *vb2_get_crypto_algorithm_file(enum vb2_crypto_algorithm alg)
{
	const struct vb2_text_vs_enum *entry =
		vb2_lookup_by_num(vb2_file_vs_crypto, alg);

	return entry ? entry->name : VB2_INVALID_ALG_NAME;
}

int vb2_lookup_hash_alg(const char *str, enum vb2_hash_algorithm *alg)
{
	const struct vb2_text_vs_enum *entry;
	uint32_t val;
	char *e;

	/* try string first */
	entry = vb2_lookup_by_name(vb2_text_vs_hash, str);
	if (entry) {
		*alg = entry->num;
		return 1;
	}

	/* fine, try number */
	val = strtoul(str, &e, 0);
	if (!*str || (e && *e))
		/* that's not a number */
		return 0;

	if (!vb2_lookup_by_num(vb2_text_vs_hash, val))
		/* That's not a valid alg */
		return 0;

	*alg = val;
	return 1;
}
