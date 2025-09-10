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
	const char *suffix = "XXXXXX";
	size_t template_len;
	char *full_path = NULL;
	int fd = -1;

	/*
	 * Check if the template is valid for mkstemp(). It must be at least
	 * 6 characters long and end in "XXXXXX".
	 */
	template_len = strlen(path_template);
	if (template_len < 6 || strcmp(path_template + template_len - 6, suffix) != 0) {
		fprintf(stderr, "Error: path_template must end with \"%s\".\n", suffix);
		return -1;
	}

	/*
	 * Pre-flight check: ensure the provided buffer is large enough for
	 * the full path before we even try to construct it.
	 */
	if (template_size < strlen(VBOOT_TMP_DIR) + 1 + template_len + 1) {
		fprintf(stderr,
			"Error: template_size is too small to hold the full "
			"temporary path.\n");
		return -1;
	}

	/* Allocate buffer from the heap to avoid stack overflow */
	full_path = malloc(template_size);
	if (!full_path) {
		fprintf(stderr, "Unable to allocate memory for temp path.\n");
		return -1;
	}

	/* Safely construct the full path template */
	snprintf(full_path, template_size, VBOOT_TMP_DIR "/%s",
		 path_template);

	fd = mkstemp(full_path);
	if (fd == -1) {
		fprintf(stderr, "Unable to create temporary file: %s\n",
			strerror(errno));
		free(full_path);
		return -1;
	}

	/* Set permissions to be readable by all, writable by owner */
	if (fchmod(fd, 0644) == -1) {
		fprintf(stderr, "Unable to set permissions on temp file: %s\n",
			strerror(errno));
		close(fd);
		unlink(full_path);
		free(full_path);
		return -1;
	}

	/*
	 * Copy the generated path back to the caller's buffer.
	 * strncpy is used because full_path may not be null-terminated if
	 * mkstemp created a path that exactly fills the buffer.
	 */
	strncpy(path_template, full_path, template_size);
	path_template[template_size - 1] = '\0';

	free(full_path);
	return fd;
}
