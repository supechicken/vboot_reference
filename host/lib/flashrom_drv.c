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
//#include "updater.h"
#include "../../futility/futility.h"
#include "flashrom.h"

// global to allow verbosity level to be injected into callback.
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
 * Tries to guess FMAP location in flash using FMAP location in `helper_image`.
 * Returns 0 and sets image->fmap_header on success, 1 on fail.
 */
static int flashrom_guess_fmap(struct firmware_image *image,
			       struct firmware_image *helper_image,
			       struct flashrom_flashctx *flashctx, uint64_t *fmap_pos,
			       size_t *fmap_len, size_t len)
{
	int r = 0;

	struct flashrom_layout *layout = NULL;

	image->fmap_header = NULL;

	if (!helper_image) {
		INFO("Helper image is not provided, proceeding without guessing.\n");
		r = 1;
		goto guess_fail;
	}

	*fmap_len = sizeof(FmapHeader) +
		    sizeof(FmapAreaHeader) * helper_image->fmap_header->fmap_nareas;
	if (*fmap_len % 4096 != 0)
		*fmap_len += 4096 - *fmap_len % 4096; /* must be aligned to 4KiB */

	*fmap_pos = (uint8_t *)helper_image->fmap_header - helper_image->data;

	INFO("Guessing FMAP at %" PRId64 " (%zu bytes)\n", *fmap_pos, *fmap_len);

	if (flashrom_layout_read_fmap_from_rom(&layout, flashctx, *fmap_pos, *fmap_len) != 0) {
		INFO("Failed to read FMAP candidate from rom, proceeding without guessing.\n");
		r = 1;
		goto guess_fail;
	}

	flashrom_layout_include_region(layout, "FMAP");
	flashrom_layout_set(flashctx, layout);
	if (flashrom_image_read(flashctx, image->data, len) != 0) {
		INFO("Failed to read rom image, proceeding without guessing.\n");
		r = 1;
		goto guess_fail;
	}

	/* verify if the fmap was guessed correctly */
	image->fmap_header = fmap_find(image->data + *fmap_pos, *fmap_len);
	if (!image->fmap_header) {
		INFO("Failed to verify FMAP candidate, proceeding without guessing.\n");
		r = 1;
		goto guess_fail;
	}

	// note: image->fmap_header is not NULL
	INFO("Guessed FMAP location correctly.\n");

guess_fail:;
	flashrom_layout_release(layout);

	return r;
}

/*
 * Prepares flash for operations.
 * Returns
 *	3 on init failure,
 *	2 on probe failure,
 *	1 on getsize failure (if len is not NULL),
 *	0 if success.
 */
static int flashrom_prepare_flash(struct flashrom_flashctx **flashctx, size_t *len,
				     struct flashrom_programmer **prog, char **tmp,
				     const char *image_programmer, char **programmer,
				     char **params)
{
	*tmp = flashrom_extract_params(image_programmer, programmer, params);

	*prog = NULL;
	*flashctx = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);
	if (flashrom_init(1) || flashrom_programmer_init(prog, *programmer, *params))
		return 3;

	if (flashrom_flash_probe(flashctx, *prog, NULL))
		return 2;

	if (len != NULL) {
		*len = flashrom_flash_getsize(*flashctx);
		if (!*len) {
			ERROR("Chip found had zero length, probing probably failed.\n");
			return 1;
		}
	}
	return 0;
}

int flashrom_read_segments(struct firmware_image *image, uint64_t offset[], size_t size[],
			   size_t segments_count, int verbosity)
{
	if (segments_count > 64) {
		ERROR("Cannot read more than 64 regions\n");
		return 3;
	}
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	size_t len = 0;
	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	r = flashrom_prepare_flash(&flashctx, &len, &prog, &tmp, image->programmer,
				      &programmer, &params);
	switch (r) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	case 1:
		goto err_cleanup;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	flashrom_layout_new(&layout);
	char name[] = "\x1\0"; /* each region has to have a name, so iterate from 1 */

	for (size_t i = 0; i < segments_count; i++) {
		INFO("Including segment %zu (%" PRId64 ", %zu) ...\n", i, offset[i], size[i]);
		if (size[i] == 0 || offset[i] + size[i] > len) {
			ERROR("Invalid segment %zu (%" PRId64 ", %zu), ignoring.\n", i,
			      offset[i], size[i]);
			continue;
		}

		if (flashrom_layout_add_region(layout, offset[i], offset[i] + size[i] - 1,
					       name)) {
			ERROR("Failed to add segment %zu (%" PRId64 ", %zu), ignoring.\n", i,
			      offset[i], size[i]);
			continue;
		}
		if (flashrom_layout_include_region(layout, name)) {
			ERROR("Failed to include segment %zu (%" PRId64 ", %zu), ignoring.\n",
			      i, offset[i], size[i]);
			continue;
		}
		name[0]++;
	}

	flashrom_layout_set(flashctx, layout);

	INFO("Reading image...\n");

	r |= flashrom_image_read(flashctx, image->data, len);

err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r;
}

/*
 * NOTE: When `regions` contains multiple regions, `region_start` and
 * `region_len` will be filled with the data of the first region.
 *
 * if `helper_image` is provided, will use it to guess
 * fmap location in flash. if fails, reverts to normal search.
 */
static int flashrom_read_image_impl(struct firmware_image *image,
				    struct firmware_image *helper_image,
				    const char *const regions[], const size_t regions_count,
				    unsigned int *region_start, unsigned int *region_len,
				    int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	*region_start = 0;
	*region_len = 0;

	uint64_t fmap_pos = 0;
	size_t fmap_len = 0;

	size_t len = 0;
	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	r = flashrom_prepare_flash(&flashctx, &len, &prog, &tmp, image->programmer,
				      &programmer, &params);
	switch (r) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	case 1:
		goto err_cleanup;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	if (!image->data) {
		image->data = calloc(1, len);
		image->size = len;
		image->file_name = strdup("<sys-flash>");
		image->fmap_header = NULL;
	} else {
		/* reading additional regions */
		fmap_pos = (uint8_t *)image->fmap_header - image->data;
		fmap_len = sizeof(FmapHeader) +
			   sizeof(FmapAreaHeader) * image->fmap_header->fmap_nareas;
		if (fmap_len % 4096 != 0)
			fmap_len += 4096 - fmap_len % 4096; /* must be aligned to 4KiB */
	}

	if (regions_count && helper_image && !image->fmap_header) {
		/* return value is disregarded as image->fmap_header will either be NULL or
		 * correctly set */
		flashrom_guess_fmap(image, helper_image, flashctx, &fmap_pos, &fmap_len, len);
	}

	if (regions_count) {
		if (image->fmap_header) {
			if (flashrom_layout_read_fmap_from_buffer(
				    &layout, flashctx, image->data + fmap_pos, fmap_len) != 0) {
				INFO("Guessing attempt failed (after successful guess!), "
				     "reverting "
				     "to normal search.\n");
				image->fmap_header = NULL;
			}
		}
		if (!image->fmap_header) {
			// usually is much faster than reading an image, even though it might
			// read the entire image (if unlucky)
			r |= flashrom_layout_read_fmap_from_rom(&layout, flashctx, 0, len);
			if (r != 0) {
				ERROR("Could not read fmap from rom, r=%d\n", r);
				r = -1;
				goto err_cleanup;
			}

			INFO("Including region 'FMAP' (because guessing failed)\n");
			r |= flashrom_layout_include_region(layout, "FMAP");
			if (r > 0) {
				ERROR("Could not include FMAP region\n");
				r = -1;
				goto err_cleanup;
			}
		}
		int i;
		for (i = 0; i < regions_count; i++) {
			// empty region causes seg fault in API.
			INFO("Including region %s\n", regions[i]);
			r |= flashrom_layout_include_region(layout, regions[i]);
			if (r > 0) {
				ERROR("Could not include region %s\n", regions[i]);
				r = -1;
				goto err_cleanup;
			}
		}
		flashrom_layout_set(flashctx, layout);
	}

	INFO("Reading image...\n");

	r |= flashrom_image_read(flashctx, image->data, len);
	if (r == 0 && regions_count)
		r |= flashrom_layout_get_region_range(layout, regions[0], region_start,
						      region_len);
err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r;
}

int flashrom_read_image(struct firmware_image *image, struct firmware_image *helper_image,
			const char *const regions[], const size_t regions_len, int verbosity)
{
	unsigned int start, len;
	return flashrom_read_image_impl(image, helper_image, regions, regions_len, &start, &len,
					verbosity);
}

int flashrom_read_region(struct firmware_image *image, const char *region,
			 int verbosity)
{
	const char * const regions[] = {region};
	unsigned int start, len;
	int r = flashrom_read_image_impl(image, NULL, regions, ARRAY_SIZE(regions),
					 &start, &len, verbosity);
	if (r != 0)
		return r;

	memmove(image->data, image->data + start, len);
	image->size = len;
	return 0;
}

int flashrom_write_image(const struct firmware_image *image,
			const char * const regions[],
			const size_t regions_len,
			const struct firmware_image *diff_image,
			int do_verify, int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	size_t len = 0;
	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	r = flashrom_prepare_flash(&flashctx, &len, &prog, &tmp, image->programmer,
				      &programmer, &params);
	switch (r) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	case 1:
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

	if (regions_len) {
		int i;
		r = flashrom_layout_read_fmap_from_buffer(
			&layout, flashctx, (const uint8_t *)image->data,
			image->size);
		if (r > 0) {
			WARN("could not read fmap from image, r=%d, "
				"falling back to read from rom\n", r);
			r = flashrom_layout_read_fmap_from_rom(
				&layout, flashctx, 0, len);
			if (r > 0) {
				ERROR("could not read fmap from rom, r=%d\n", r);
				r = -1;
				goto err_cleanup;
			}
		}
		for (i = 0; i < regions_len; i++) {
			INFO(" including region '%s'\n", regions[i]);
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
	} else if (image->size != len) {
		r = -1;
		goto err_cleanup;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNWRITABLE_REGIONS, true);
	flashrom_flag_set(flashctx, FLASHROM_FLAG_VERIFY_WHOLE_CHIP, false);
	flashrom_flag_set(flashctx, FLASHROM_FLAG_VERIFY_AFTER_WRITE,
			  do_verify);

	r |= flashrom_image_write(flashctx, image->data, image->size,
				  diff_image ? diff_image->data : NULL);

err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r;
}

int flashrom_get_wp(const char *prog_with_params, bool *wp_mode,
		    uint32_t *wp_start, uint32_t *wp_len, int verbosity)
{
	int ret = -1;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	struct flashrom_wp_cfg *cfg = NULL;

	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	ret = flashrom_prepare_flash(&flashctx, NULL, &prog, &tmp, prog_with_params,
				      &programmer, &params);
	switch (ret) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	}

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

	return ret;
}

int flashrom_set_wp(const char *prog_with_params, bool wp_mode,
		    uint32_t wp_start, uint32_t wp_len, int verbosity)
{
	int ret = 1;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	struct flashrom_wp_cfg *cfg = NULL;

	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	ret = flashrom_prepare_flash(&flashctx, NULL, &prog, &tmp, prog_with_params,
				      &programmer, &params);
	switch (ret) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	}

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

	return ret;
}

int flashrom_get_info(const char *prog_with_params,
		      char **vendor, char **name,
		      uint32_t *vid, uint32_t *pid,
		      uint32_t *flash_len, int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	r = flashrom_prepare_flash(&flashctx, NULL, &prog, &tmp, prog_with_params,
				      &programmer, &params);
	switch (r) {
	case 3:
		goto err_init;
	case 2:
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
	return r;
}

int flashrom_get_size(const char *prog_with_params,
		      uint32_t *flash_len, int verbosity)
{
	int r = 0;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;

	r = flashrom_prepare_flash(&flashctx, NULL, &prog, &tmp, prog_with_params,
				      &programmer, &params);
	switch (r) {
	case 3:
		goto err_init;
	case 2:
		goto err_probe;
	}

	*flash_len = flashrom_flash_getsize(flashctx);

	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r;
}
