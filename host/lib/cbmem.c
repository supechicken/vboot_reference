/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbmem.h"

#include "subprocess.h"
#include "host_misc.h"

static const char *get_cbmem_path(void)
{
	static const char *cbmem = NULL;

	if (cbmem)
		return cbmem;

	const char *env_cbmem = getenv(ENV_CBMEM);
	if (env_cbmem && env_cbmem[0] != '\0') {
		cbmem = strdup(cbmem);
		return cbmem;
	}

	cbmem = DEFAULT_CBMEM;
	return cbmem;
}

int cbmem_get_rawdump(const char *id, uint8_t *buffer, size_t *count) {
	int status;
	const char *cbmem = get_cbmem_path();

	struct subprocess_target output = {
		.type = TARGET_BUFFER,
		.buffer = {
			.buf = (char *)buffer,
			.size = *count,
		},
	};
	const char *const argv[] = {
		cbmem, "-r", id, NULL
	};

	status = subprocess_run(argv, &subprocess_null, &output,
				&subprocess_null);

	if (status != 0) {
		fprintf(stderr, "error: 'cbmem rawdump %s' failed: %d\n", id, status);
		return 1;
	}

	*count = output.buffer.bytes_consumed;

	return 0;
}
