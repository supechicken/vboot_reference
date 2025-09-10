/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host-side temporary file creation functions.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tempfile.h"

int create_vboot_temp_file(char *path_template, size_t template_size)
{
	char full_path[template_size];
	int fd;

	/* Safely construct the full path template */
	snprintf(full_path, sizeof(full_path), VBOOT_TMP_DIR "/%s",
		 path_template);

	fd = mkstemp(full_path);
	if (fd == -1) {
		fprintf(stderr, "Unable to create temporary file: %s\n",
			strerror(errno));
		return -1;
	}

	/* Set permissions to be readable by all, writable by owner */
	if (fchmod(fd, 0644) == -1) {
		fprintf(stderr, "Unable to set permissions on temp file: %s\n",
			strerror(errno));
		close(fd);
		unlink(full_path);
		return -1;
	}

	/*
	 * Copy the generated path back to the caller's buffer.
	 * strncpy is used because full_path may not be null-terminated if
	 * mkstemp created a path that exactly fills the buffer.
	 */
	strncpy(path_template, full_path, template_size);
	path_template[template_size - 1] = '\0';

	return fd;
}
