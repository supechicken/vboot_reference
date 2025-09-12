/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* For strdup */
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "2api.h"
#include "2return_codes.h"
#include "host_misc.h"
#include "flashrom.h"
#include "subprocess.h"

#define FLASHROM_EXEC_NAME "flashrom"

/**
 * Helper to create a temporary file, and optionally write some data
 * into it.
 *
 * @param data		If data needs to be written to the file, a
 *			pointer to the buffer.  Pass NULL to just
 *			create an empty temporary file.
 * @param data_size	The size of the buffer to write, if applicable.
 * @param path_out	An output pointer for the filename.  Caller
 *			should free.
 *
 * @return VB2_SUCCESS on success, or a relevant error.
 */
static vb2_error_t write_temp_file(const uint8_t *data, uint32_t data_size,
				   char **path_out)
{
	int fd;
	ssize_t write_rv;
	vb2_error_t rv;
	char *path;
	mode_t umask_save;

	*path_out = NULL;

	path = strdup(VBOOT_TMP_DIR "/vb2_flashrom.XXXXXX");

	/* Set the umask before mkstemp for security considerations. */
	umask_save = umask(077);
	fd = mkstemp(path);
	umask(umask_save);
	if (fd < 0) {
		rv = VB2_ERROR_WRITE_FILE_OPEN;
		goto fail;
	}

	while (data && data_size > 0) {
		write_rv = write(fd, data, data_size);
		if (write_rv < 0) {
			close(fd);
			unlink(path);
			rv = VB2_ERROR_WRITE_FILE_DATA;
			goto fail;
		}

		data_size -= write_rv;
		data += write_rv;
	}

	close(fd);
	*path_out = path;
	return VB2_SUCCESS;

 fail:
	free(path);
	return rv;
}

static vb2_error_t run_flashrom(const char *const argv[])
{
	int status = subprocess_run(argv, &subprocess_null, &subprocess_null,
				    &subprocess_null);
	if (status) {
		fprintf(stderr, "Flashrom invocation failed (exit status %d):",
			status);

		for (const char *const *argp = argv; *argp; argp++)
			fprintf(stderr, " %s", *argp);

		fprintf(stderr, "\n");
		return VB2_ERROR_FLASHROM;
	}

	return VB2_SUCCESS;
}

static const char *get_verbosity_flag(int verbosity)
{
	if (verbosity < 2)
		fprintf(stderr, "INFO: %s: Flashrom cli doesn't support verbosity level < 2. "
			"Got verbosity: %d.\n", __func__, verbosity);
	else if (verbosity == 3)
		return "-V";
	else if (verbosity == 4)
		return "-VV";
	else if (verbosity >= 5)
		return "-VVV";
	return NULL;
}

static int flashrom_read_image_impl(struct firmware_image *image, const char * const regions[],
				    const size_t regions_len, bool extract_region,
				    int verbosity)
{
	char *tmpfile;
	char region_param[PATH_MAX];
	vb2_error_t rv;

	if (regions_len != 1 && extract_region) {
		fprintf(stderr, "ERROR: %s: Invalid number of regions for extraction. "
			"The current implementation only supports extracting a single region, "
			"but %zu were provided.\n", __func__, region_len);
		return VB2_ERROR_FLASHROM;
	}

	/*
	 * flashrom -p <programmer> -r <file> [-V[V[V]]]
	 * or
	 * flashrom -p <programmer> -r <file> -i <region1> -i <region2> ... [-V[V[V]]]
	 * or
	 * flashrom -p <programmer> -r -i <region>:<file> [-V[V[V]]]
	 */
	size_t argc = 6 + (regions_len * 2);
	const char **argv = callc(argc + 1, sizeof(*argv));
	if (!argv) {
		fprintf(stderr, "ERROR: %s: Memory allocation for argv failed.\n", __func__);
		return VB2_ERROR_FLASHROM;
	}
	int i = 0;

	image->data = NULL;
	image->size = 0;

	VB2_TRY(write_temp_file(NULL, 0, &tmpfile));

	argv[i++] = FLASHROM_EXEC_NAME;
	argv[i++] = "-p";
	argv[i++] = image->programmer;
	argv[i++] = "-r";

	if (extract_region) {
		snprintf(region_param, sizeof(region_param), "%s:%s", region, tmpfile);
		argv[i++] = "-i";
		argv[i++] = region_paraml;
	} else {
		argv[i++] = tmpfile;
		for (size_t j = 0; j < regions_len; j++) {
			argv[i++] = "-i";
			argv[i++] = regions[j];
		}
	}

	argv[i++] = get_verbosity_flag(verbosity);

	rv = run_flashrom(argv);
	if (rv == VB2_SUCCESS)
		rv = vb2_read_file(tmpfile, &image->data, &image->size);

	unlink(tmpfile);
	free(tmpfile);
	return rv;
}

int flashrom_read_image(struct firmware_image *image,
			const char * const regions[],
			const size_t regions_len,
			int verbosity)
{
	return (int)flashrom_read_image_impl(image, regions, regions_len, false, verbosity);
}

int flashrom_read_region(struct firmware_image *image, const char *region,
			 int verbosity)
{
	const char * const regions[] = {region};
	return (int)flashrom_read_image_impl(image, regions, ARRAY_SIZE(regions), true,
					     verbosity);
}

vb2_error_t flashrom_read(struct firmware_image *image, const char *region)
{
	return (vb2_error_t)flashrom_read_region(image, region, FLASHROM_MSG_INFO);
}

vb2_error_t flashrom_write(struct firmware_image *image, const char *region)
{
	char *tmpfile;
	char region_param[PATH_MAX];
	vb2_error_t rv;

	VB2_TRY(write_temp_file(image->data, image->size, &tmpfile));

	if (region)
		snprintf(region_param, sizeof(region_param), "%s:%s", region,
			 tmpfile);

	const char *const argv[] = {
		FLASHROM_EXEC_NAME,
		"-p",
		image->programmer,
		"--noverify-all",
		"-w",
		region ? "-i" : tmpfile,
		region ? region_param : NULL,
		NULL,
	};

	rv = run_flashrom(argv);
	unlink(tmpfile);
	free(tmpfile);
	return rv;
}
