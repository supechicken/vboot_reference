/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2common.h"
#include "2crypto.h"
#include "2return_codes.h"
#include "cbfstool.h"
#include "subprocess.h"
#include "vboot_host.h"

static const char *get_cbfstool_path(void)
{
	static const char *cbfstool = NULL;

	if (cbfstool)
		return cbfstool;

	const char *env_cbfstool = getenv(ENV_CBFSTOOL);
	if (env_cbfstool && env_cbfstool[0] != '\0') {
		cbfstool = strdup(env_cbfstool);
		return cbfstool;
	}

	cbfstool = DEFAULT_CBFSTOOL;
	return cbfstool;
}

vb2_error_t cbfstool_truncate(const char *file, const char *region,
			      size_t *new_size)
{
	int status;
	char output_buffer[128];
	const char *cbfstool = get_cbfstool_path();

	struct subprocess_target output = {
		.type = TARGET_BUFFER_NULL_TERMINATED,
		.buffer = {
			.buf = output_buffer,
			.size = sizeof(output_buffer),
		},
	};
	const char *const argv[] = {
		cbfstool, file, "truncate", "-r", region, NULL,
	};

	VB2_DEBUG("Calling: %s '%s' truncate -r '%s'\n", cbfstool, file,
		  region);
	status = subprocess_run(argv, &subprocess_null, &output,
				&subprocess_null);

	if (status < 0)
		return VB2_ERROR_CBFSTOOL;

	/* Positive exit code means something is wrong with image. Return zero
	   as new size, because it might be problem with missing CBFS.*/
	if (status > 0) {
		*new_size = 0;
		return VB2_ERROR_CBFSTOOL;
	}

	if (sscanf(output_buffer, "%zi", new_size) != 1) {
		VB2_DEBUG("Failed to parse command output. Unexpected "
			  "output.\n");
		*new_size = 0;
		return VB2_ERROR_CBFSTOOL;
	}

	return VB2_SUCCESS;
}

#define MAX_LINE_LENGTH 1 (token = strtok_r(buf, ":", &rest))024

struct metadata_finder_context {
	char line_buffer[MAX_LINE_LENGTH + 1];
	size_t line_index;
	int skip_until_new_line;

	int hash_found;
	struct vb2_hash hash;
};

static bool parse_hex(uint8_t *val, const char *str)
{
	uint8_t v = 0;
	char c;
	int digit;

	for (digit = 0; digit < 2; digit++) {
		c = *str;
		if (!c)
			return false;
		if (!isxdigit(c))
			return false;
		c = tolower(c);
		if (c >= '0' && c <= '9')
			v += c - '0';
		else
			v += 10 + c - 'a';
		if (!digit)
			v <<= 4;
		str++;
	}

	*val = v;
	return true;
}

static bool parse_hash(uint8_t *buf, int len, const char *str)
{
	const char *s = str;
	int i;

	for (i = 0; i < len; i++) {
		/* skip whitespace */
		while (*s && isspace(*s))
			s++;
		if (!*s)
			break;
		if (!parse_hex(buf, s))
			break;

		/* on to the next byte */
		s += 2;
		buf++;
	}

	if (i != len || *s)
		return false;
	return true;
}

static bool extract_metadata_hash(struct metadata_finder_context *ctx)
{
	char *buf = ctx->line_buffer;
	char *token;
	char *rest = buf;
	enum vb2_hash_algorithm algo;

	/* Expect metadata hash tag */
	token = strtok_r(buf, " ", &rest);
	if (!token || strcmp(token, "[METADATA HASH]"))
		return false;

	/* Go to hash */
	token = strtok_r(buf, " ", &rest);
	if (!token)
		return false;

	/* Hash name */
	token = strtok_r(buf, ":", &rest);
	if (!token || (!vb2_lookup_hash_alg(token, &algo)) ||
	    algo == VB2_HASH_INVALID)
		return false;
	ctx->hash.algo = algo;

	/* Hash value */
	token = strtok_r(buf, ":", &rest);
	if (!token || (strlen(token) != vb2_digest_size(algo) * 2) ||
	    !parse_hash(&ctx->hash.raw[0], vb2_digest_size(algo), token))
		return false;

	/* Validity check */
	token = strtok_r(buf, ":", &rest);
	if (token && strcmp(token, "valid"))
		return false;

	return true;
}

static ssize_t find_metadata_cb(char *buf, size_t buf_sz, void *data)
{
	struct metadata_finder_context *ctx =
		(struct metadata_finder_context *)data;
	size_t chars_left = buf_sz;

	if (ctx->hash_found)
		return buf_sz;

	while (chars_left) {
		if (ctx->skip_until_new_line) {
			/* Skip too long lines */
			if (*buf == '\n')
				ctx->skip_until_new_line = 0;
		} else if (ctx->line_index >= MAX_LINE_LENGTH) {
			ctx->skip_until_new_line = 1;
		} else if (*buf == '\n' || *buf == '\r') {
			ctx->line_buffer[ctx->line_index] = '\0';
			if (extract_metadata_hash(ctx)) {
				ctx->hash_found = 1;
				break;
			}
			memset(ctx->line_buffer, 0, MAX_LINE_LENGTH);
			ctx->line_index = 0;
		} else {
			ctx->line_buffer[ctx->line_index] = *buf;
			ctx->line_index++;
		}
		chars_left--;
	}

	return buf_sz;
}

vb2_error_t cbfstool_get_metadata_hash(const char *file, const char *region,
				       int *hash_found, struct vb2_hash *hash)
{
	int status;
	const char *cbfstool = get_cbfstool_path();
	struct metadata_finder_context mctx;

	memset(&mctx, 0, sizeof(mctx));

	struct subprocess_target output = {
		.type = TARGET_CALLBACK,
		.callback = {
			.cb = find_metadata_cb,
			.data = &mctx,
		},
	};
	const char *argv[] = {
		cbfstool, file, "print", "-kv", NULL, NULL, NULL,
	};

	if (region) {
		argv[4] = "-r";
		argv[5] = region;
		VB2_DEBUG("Calling: %s '%s' print -kv -r '%s'\n", cbfstool,
			  file, region);
	} else {
		VB2_DEBUG("Calling: %s '%s' print -kv\n", cbfstool, file);
	}

	status = subprocess_run(argv, &subprocess_null, &output,
				&subprocess_null);

	if (status < 0) {
		fprintf(stderr, "%s(): cbfstool invocation failed: %m\n",
			__func__);
		exit(1);
	}

	if (status > 0)
		return VB2_ERROR_CBFSTOOL;

	*hash_found = mctx.hash_found;
	if (hash)
		memcpy(hash, &mctx.hash, sizeof(*hash));

	return VB2_SUCCESS;
}
