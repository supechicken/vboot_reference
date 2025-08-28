/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stub implementation for flashrom functions. Used in builds where
 * flashrom support is disabled.
 */

#include <stdio.h>

#include "../../futility/futility.h"
#include "flashrom.h"

/*
 * A static helper function to print a consistent error message.
 * This informs the user that the feature is unavailable in their build.
 */
static void print_stub_error(void)
{
	ERROR("This futility binary was built without flashrom support.\n");
}

int flashrom_read_image(struct firmware_image *image,
			const char *const regions[],
			const size_t regions_len, int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_read_region(struct firmware_image *image, const char *region,
			 int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_write_image(const struct firmware_image *image,
			 const char *const regions[],
			 const size_t regions_len,
			 const struct firmware_image *diff_image,
			 int do_verify, int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_get_wp(const char *prog_with_params, bool *wp_mode,
		    uint32_t *wp_start, uint32_t *wp_len, int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_set_wp(const char *prog_with_params, bool wp_mode,
		    uint32_t wp_start, uint32_t wp_len, int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_get_info(const char *prog_with_params, char **vendor, char **name,
		      uint32_t *vid, uint32_t *pid, uint32_t *flash_len,
		      int verbosity)
{
	print_stub_error();
	return -1;
}

int flashrom_get_size(const char *prog_with_params, uint32_t *flash_len,
		      int verbosity)
{
	print_stub_error();
	return -1;
}
