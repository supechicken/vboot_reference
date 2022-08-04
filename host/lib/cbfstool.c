/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2common.h"
#include "2crypto.h"
#include "2return_codes.h"
#include "cbfstool.h"
#include "host_misc.h"
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

	if (status < 0) {
		fprintf(stderr, "%s(): cbfstool invocation failed: %m\n",
			__func__);
		exit(1);
	}

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


/* Requires null-terminated buffer */
static bool extract_metadata_hash(char *buf, struct vb2_hash *hash)
{
	char *rest_line = buf;
	char *line = strtok_r(buf, "\n", &rest_line);
	char *token;
	char *rest = buf;
	enum vb2_hash_algorithm algo;
	const char line_prefix[] = "[METADATA HASH]";
	const size_t line_prefix_sz = ARRAY_SIZE(line_prefix) - 1;

	while (line) {
		/* Expect metadata hash tag */
		if (strncmp(line, line_prefix, line_prefix_sz))
			goto no_hash;

		/* Skip metadata hash tag */
		token = strtok_r(line, " \t", &rest);
		if (!token)
			goto no_hash;
		token = strtok_r(rest, " \t", &rest);
		if (!token)
			goto no_hash;

		/* Hash algorithm name */
		token = strtok_r(rest, ":", &rest);
		if (!token || (!vb2_lookup_hash_alg(token, &algo)) ||
		    algo == VB2_HASH_INVALID)
			goto no_hash;
		hash->algo = algo;

		/* Hash value */
		token = strtok_r(rest, ":", &rest);
		if (!token || (strlen(token) != vb2_digest_size(algo) * 2) ||
		    !parse_hash(&hash->raw[0], vb2_digest_size(algo), token))
			goto no_hash;

		/* Validity check */
		token = strtok_r(rest, ":", &rest);
		if (token && strcmp(token, "valid"))
			goto no_hash;

		return true;
no_hash:
		line = strtok_r(rest_line, "\r\n", &rest_line);
	}

	return false;
}

vb2_error_t cbfstool_get_metadata_hash(const char *file, const char *region,
				       struct vb2_hash *hash)
{
	int status;
	const char *cbfstool = get_cbfstool_path();
	const size_t data_buffer_sz = 1024 * 1024;
	char *data_buffer = malloc(data_buffer_sz);
	vb2_error_t rv = VB2_ERROR_CBFSTOOL;

	if (!data_buffer)
		goto done;

	memset(hash, 0, sizeof(*hash));
	hash->algo = VB2_HASH_INVALID;

	struct subprocess_target output = {
		.type = TARGET_BUFFER_NULL_TERMINATED,
		.buffer = {
			.buf = data_buffer,
			.size = data_buffer_sz,
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
		goto done;

	if (!extract_metadata_hash(data_buffer, hash))
		goto done;

	rv = VB2_SUCCESS;
done:
	free(data_buffer);
	return rv;
}

/* Requires null-terminated buffer */
static char *extract_config_value(char *buf, const char *config_field)
{

	char *rest_line = NULL;
	char *line = strtok_r(buf, "\r\n", &rest_line);
	char *token;
	char *rest = buf;

	while (line) {
		token = strtok_r(line, "=", &rest);
		if (!token)
			goto skip;

		if (strcmp(token, config_field))
			goto skip;

		token = strtok_r(rest, "=", &rest);
		if (token)
			return token;

skip:
		line = strtok_r(rest_line, "\n", &rest_line);
	}

	return NULL;
}

vb2_error_t cbfstool_get_config_value(const char *file, const char *region,
				      const char *config_field, char **value)
{
	int status;
	const char *cbfstool = get_cbfstool_path();
	const size_t data_buffer_sz = 1024 * 1024;
	char *data_buffer = malloc(data_buffer_sz);
	vb2_error_t rv = VB2_ERROR_CBFSTOOL;

	*value = NULL;

	if (!data_buffer)
		goto done;

	struct subprocess_target output = {
		.type = TARGET_BUFFER_NULL_TERMINATED,
		.buffer = {
			.buf = data_buffer,
			.size = data_buffer_sz,
		},
	};
	const char *argv[] = {
		cbfstool, file, "extract", "-n", "config", "-f", "/dev/stdout",
		NULL, NULL, NULL
	};

	if (region) {
		argv[7] = "-r";
		argv[8] = region;
		VB2_DEBUG("Calling: %s '%s' extract -n config -f /dev/stdout -r"
			  " '%s'\n",
			  cbfstool, file, region);
	} else {
		VB2_DEBUG("Calling: %s '%s' extract -n config -f /dev/stdout\n",
			  cbfstool, file);
	}

	status = subprocess_run(argv, &subprocess_null, &output,
				&subprocess_null);

	if (status < 0) {
		fprintf(stderr, "%s(): cbfstool invocation failed: %m\n",
			__func__);
		exit(1);
	}

	if (status > 0)
		goto done;

	char *value_found = extract_config_value(data_buffer, config_field);
	if (!value_found)
		goto done;

	*value = strdup(value_found);

	rv = VB2_SUCCESS;
done:
	free(data_buffer);
	return rv;
}
