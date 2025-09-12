/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The utility functions for firmware updater.
 */

#include <libflashrom.h>

#include "2common.h"
#include "crossystem.h"
#include "host_misc.h"
#include "util_misc.h"
#include "../../futility/futility.h"
#include "flashrom.h"

/* global to allow verbosity level to be injected into callback. */
static enum flashrom_log_level g_verbose_screen = FLASHROM_MSG_INFO;

static int flashrom_print_cb(enum flashrom_log_level level, const char *fmt,
			     va_list ap)
{
	int ret = 0;
	FILE *output_type = (level < FLASHROM_MSG_INFO) ? stderr : stdout;

	if (level > g_verbose_screen)
		return ret;

	ret = vfprintf(output_type, fmt, ap);
	/* msg_*spew often happens inside chip accessors
	 * in possibly time-critical operations.
	 * Don't slow them down by flushing.
	 */
	if (level != FLASHROM_MSG_SPEW)
		fflush(output_type);

	return ret;
}

static char *flashrom_extract_params(const char *str, char **prog, char **params)
{
	char *tmp = strdup(str);
	*prog = strtok(tmp, ":");
	*params = strtok(NULL, "");
	return tmp;
}

/*
 * NOTE: When `regions` contains multiple regions, `region_start` and
 * `region_len` will be filled with the data of the first region.
 */
static vb2_error_t flashrom_read_image_impl(struct firmware_image *image,
					    const char * const regions[],
					    const size_t regions_len,
					    unsigned int *region_start,
					    unsigned int *region_len, int verbosity)
{
	int r = 0;
	size_t len = 0;
	*region_start = 0;
	*region_len = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(image->programmer, &programmer, &params);

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1)
		|| flashrom_programmer_init(&prog, programmer, params)) {
		r = -1;
		goto err_init;
	}
	if (flashrom_flash_probe(&flashctx, prog, NULL)) {
		r = -1;
		goto err_probe;
	}

	len = flashrom_flash_getsize(flashctx);
	if (!len) {
		ERROR("Chip found had zero length, probing probably failed.\n");
		r = -1;
		goto err_probe;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	if (regions_len) {
		int i;
		r = flashrom_layout_read_fmap_from_rom(
			&layout, flashctx, 0, len);
		if (r > 0) {
			ERROR("could not read fmap from rom, r=%d\n", r);
			r = -1;
			goto err_cleanup;
		}
		for (i = 0; i < regions_len; i++) {
			// empty region causes seg fault in API.
			r |= flashrom_layout_include_region(layout, regions[i]);
			if (r > 0) {
				ERROR("could not include region = '%s'\n",
				      regions[i]);
				r = -1;
				goto err_cleanup;
			}
		}
		flashrom_layout_set(flashctx, layout);
	}

	image->data = calloc(1, len);
	if (!image->data) {
		ERROR("could not allocate image data (%zu bytes)\n", len);
		r = -1;
		goto err_cleanup;
	}
	image->size = len;
	image->file_name = strdup("<sys-flash>");

	r |= flashrom_image_read(flashctx, image->data, len);

	if (r == 0 && regions_len)
		r |= flashrom_layout_get_region_range(layout, regions[0],
						      region_start, region_len);

err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	if (r) {
		free(image->data);
		free(image->file_name);
	}
	return r ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}

vb2_error_t flashrom_read_image(struct firmware_image *image, const char * const regions[],
				const size_t regions_len, int verbosity)
{
	unsigned int start, len;
	return flashrom_read_image_impl(image, regions, regions_len, &start, &len, verbosity);
}

vb2_error_t flashrom_read_region(struct firmware_image *image, const char *region,
				 int verbosity)
{
	const char * const regions[] = {region};
	unsigned int start, len;

	if (region == NULL) {
		ERROR("region name must be specified\n");
		return VB2_ERROR_FLASHROM;
	}

	vb2_error_t r = flashrom_read_image_impl(image, regions, ARRAY_SIZE(regions),
						 &start, &len, verbosity);
	if (r != VB2_SUCCESS)
		return r;

	memmove(image->data, image->data + start, len);
	image->size = len;
	return VB2_SUCCESS;
}

/*
 * Internal implementation for flashrom writes.
 *
 * This function handles two distinct cases:
 *
 * 1. Full-layout buffer (image->size == flash_size):
 * The image is passed directly to libflashrom. The `regions` array (if non-zero) is used to
 * specify which parts of the full buffer to write.
 *
 * 2. Fitted-region buffer (image->size != flash_size):
 * This function allocates a new full-size buffer, copies the small `image` data into the
 * correct FMAP offset, and then passes that new full buffer to libflashrom.
 */
static vb2_error_t flashrom_write_image_impl(const struct firmware_image *image,
					     const char * const regions[],
					     const size_t regions_len,
					     const struct firmware_image *diff_image,
					     bool do_verify, int verbosity)
{
	int r = 0;
	size_t len = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(image->programmer, &programmer, &params);

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	/*
	 * `image_to_write` points to the final, full-size buffer that will be passed to
	 * libflashrom.
	 *
	 * It defaults to `image` (for the full-layout case).
	 * If a fitted-region buffer is detected, `image_full` will be allocated and this
	 * pointer will be updated to point to it.
	 */
	const struct firmware_image *image_to_write = image;
	struct firmware_image image_full = *image;
	image_full.data = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1)
		|| flashrom_programmer_init(&prog, programmer, params)) {
		r = -1;
		goto err_init;
	}
	if (flashrom_flash_probe(&flashctx, prog, NULL)) {
		r = -1;
		goto err_probe;
	}

	len = flashrom_flash_getsize(flashctx);
	if (!len) {
		ERROR("Chip found had zero length, probing probably failed.\n");
		r = -1;
		goto err_cleanup;
	}

	if (diff_image) {
		if (diff_image->size != image->size) {
			ERROR("diff_image->size != image->size");
			r = -1;
			goto err_cleanup;
		}
	}

	/* Must occur before attempting to read FMAP from SPI flash. */
	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	/* image->size != len implies a fitted-region buffer update. This mode must target
	   exactly one region. */
	if (image->size != len) {
		if (regions_len != 1) {
			ERROR("image size (%u) does not match flash size (%zu). "
			      "Fitted-region update requires exactly 1 region, but got %zu.\n",
			      image->size, len, regions_len);
			r = -1;
			goto err_cleanup;
		}
		r = flashrom_layout_read_fmap_from_rom(&layout, flashctx, 0, len);
		if (r > 0) {
			ERROR("could not read fmap from rom, r=%d\n", r);
			r = -1;
			goto err_cleanup;
		}
		/* Get the region_start and region_len. */
		if (flashrom_layout_include_region(layout, regions[0])) {
			ERROR("region '%s' not found in FMAP\n", regions[0]);
			r = -1;
			goto err_cleanup;
		}
		unsigned int region_start, region_len;
		if (flashrom_layout_get_region_range(layout, regions[0], &region_start,
						     &region_len)) {
			ERROR("could not get range for region '%s'\n", regions[0]);
			r = -1;
			goto err_cleanup;
		}
		if (image->size != region_len) {
			ERROR("image size (%u) does not match region '%s' size (%u)\n",
			      image->size, regions[0], region_len);
			r = -1;
			goto err_cleanup;
		}
		/* Prepare the full-layout image buffer. */
		image_full.size = len;
		image_full.data = malloc(len);
		if (!image_full.data) {
			ERROR("could not allocate memory for full image (%zu bytes)\n", len);
			r = -1;
			goto err_cleanup;
		}
		memset(image_full.data, 0xff, len);
		memcpy(image_full.data + region_start, image->data, image->size);
		image_to_write = &image_full;
	} else {
		r = flashrom_layout_read_fmap_from_buffer(&layout, flashctx,
							  (const uint8_t *)image->data,
							  image->size);
		if (r > 0) {
			WARN("could not read fmap from image, r=%d, "
				"falling back to read from rom\n", r);
			r = flashrom_layout_read_fmap_from_rom(&layout, flashctx, 0, len);
			if (r > 0) {
				ERROR("could not read fmap from rom, r=%d\n", r);
				r = -1;
				goto err_cleanup;
			}
		}
		int i;
		for (i = 0; i < regions_len; i++) {
			INFO(" including region '%s'\n", regions[i]);
			/* empty region causes seg fault in API. */
			r |= flashrom_layout_include_region(layout, regions[i]);
			if (r > 0) {
				ERROR("could not include region = '%s'\n",
				      regions[i]);
				r = -1;
				goto err_cleanup;
			}
		}
	}
	if (regions_len)
		flashrom_layout_set(flashctx, layout);

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNWRITABLE_REGIONS, true);
	flashrom_flag_set(flashctx, FLASHROM_FLAG_VERIFY_WHOLE_CHIP, false);
	flashrom_flag_set(flashctx, FLASHROM_FLAG_VERIFY_AFTER_WRITE, do_verify);

	r |= flashrom_image_write(flashctx, image_to_write->data, image_to_write->size,
				  diff_image ? diff_image->data : NULL);

err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(image_full.data);
	free(tmp);
	return r ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}

vb2_error_t flashrom_write_image(const struct firmware_image *image,
				 const char * const regions[], const size_t regions_len,
				 const struct firmware_image *diff_image,
				 bool do_verify, int verbosity)
{
	return flashrom_write_image_impl(image, regions, regions_len, diff_image, do_verify,
					 verbosity);
}

vb2_error_t flashrom_write_region(const struct firmware_image *image, const char *region,
				  bool do_verify, int verbosity)
{
	const char *const regions[] = {region};

	if (region == NULL) {
		ERROR("region name must be specified\n");
		return VB2_ERROR_FLASHROM;
	}
	return flashrom_write_image(image, regions, ARRAY_SIZE(regions), NULL, do_verify,
				    verbosity);
}

vb2_error_t flashrom_get_wp(const char *prog_with_params, bool *wp_mode,
			    uint32_t *wp_start, uint32_t *wp_len, int verbosity)
{
	int ret = -1;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	struct flashrom_wp_cfg *cfg = NULL;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(prog_with_params, &programmer,
					    &params);

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1)
		|| flashrom_programmer_init(&prog, programmer, params))
		goto err_init;

	if (flashrom_flash_probe(&flashctx, prog, NULL))
		goto err_probe;

	if (flashrom_wp_cfg_new(&cfg) != FLASHROM_WP_OK)
		goto err_cleanup;

	if (flashrom_wp_read_cfg(cfg, flashctx) != FLASHROM_WP_OK)
		goto err_read_cfg;

	/* size_t tmp variables for libflashrom compatibility */
	size_t tmp_wp_start, tmp_wp_len;
	flashrom_wp_get_range(&tmp_wp_start, &tmp_wp_len, cfg);

	if (wp_start != NULL)
		*wp_start = tmp_wp_start;
	if (wp_start != NULL)
		*wp_len = tmp_wp_len;
	if (wp_mode != NULL)
		*wp_mode = flashrom_wp_get_mode(cfg) != FLASHROM_WP_MODE_DISABLED;

	ret = 0;

err_read_cfg:
	flashrom_wp_cfg_release(cfg);

err_cleanup:
	flashrom_flash_release(flashctx);

err_probe:
	if (flashrom_programmer_shutdown(prog))
		ret = -1;

err_init:
	free(tmp);

	return ret ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}

vb2_error_t flashrom_set_wp(const char *prog_with_params, bool wp_mode,
			    uint32_t wp_start, uint32_t wp_len, int verbosity)
{
	int ret = 1;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	struct flashrom_wp_cfg *cfg = NULL;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(prog_with_params, &programmer,
					    &params);

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1)
		|| flashrom_programmer_init(&prog, programmer, params))
		goto err_init;

	if (flashrom_flash_probe(&flashctx, prog, NULL))
		goto err_probe;

	if (flashrom_wp_cfg_new(&cfg) != FLASHROM_WP_OK)
		goto err_cleanup;

	enum flashrom_wp_mode mode = wp_mode ?
			FLASHROM_WP_MODE_HARDWARE : FLASHROM_WP_MODE_DISABLED;
	flashrom_wp_set_mode(cfg, mode);
	flashrom_wp_set_range(cfg, wp_start, wp_len);

	if (flashrom_wp_write_cfg(flashctx, cfg) != FLASHROM_WP_OK)
		goto err_write_cfg;

	ret = 0;

err_write_cfg:
	flashrom_wp_cfg_release(cfg);

err_cleanup:
	flashrom_flash_release(flashctx);

err_probe:
	if (flashrom_programmer_shutdown(prog))
		ret = 1;

err_init:
	free(tmp);
	return ret ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}

vb2_error_t flashrom_get_info(const char *prog_with_params, char **vendor, char **name,
			      uint32_t *vid, uint32_t *pid, uint32_t *flash_len, int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(prog_with_params,
					    &programmer, &params);

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1) ||
	    flashrom_programmer_init(&prog, programmer, params)) {
		r = -1;
		goto err_init;
	}
	if (flashrom_flash_probe(&flashctx, prog, NULL)) {
		r = -1;
		goto err_probe;
	}

	struct flashrom_flashchip_info info;
	flashrom_flash_getinfo(flashctx, &info);

	*vendor = strdup(info.vendor);
	*name = strdup(info.name);
	*vid = info.manufacture_id;
	*pid = info.model_id;
	*flash_len = info.total_size * 1024;

	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}

vb2_error_t flashrom_get_size(const char *prog_with_params, uint32_t *flash_len, int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *programmer, *params;
	char *tmp = flashrom_extract_params(prog_with_params,
					    &programmer, &params);

	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	if (flashrom_init(1) ||
	    flashrom_programmer_init(&prog, programmer, params)) {
		r = -1;
		goto err_init;
	}
	if (flashrom_flash_probe(&flashctx, prog, NULL)) {
		r = -1;
		goto err_probe;
	}

	*flash_len = flashrom_flash_getsize(flashctx);

	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r ? VB2_ERROR_FLASHROM : VB2_SUCCESS;
}
