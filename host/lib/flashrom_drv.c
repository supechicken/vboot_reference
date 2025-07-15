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

/**
 * Attempts to locate FMAP in flash using `helper_fmap`. The function will look for FMAP in
 * flash at the offset which is stored in the FMAP section of `helper_fmap`.
 *
 * If `image` already has an FMAP header, or `helper_fmap` is not provided, nothing will be
 * done. Otherwise, if FMAP is located successfully, `image->fmap_header` will be set to the
 * located FMAP, `fmap_pos` and `fmap_len` will be set to its offset and size. Otherwise,
 * `image->fmap_header` will stay NULL.
 */
static void locate_fmap_using_helper_fmap(struct flashrom_flashctx *flashctx,
					  struct firmware_image *image, FmapHeader *helper_fmap,
					  uint64_t *fmap_pos, size_t *fmap_len,
					  size_t flash_len)
{
	if (image->fmap_header || !helper_fmap)
		return;

	struct flashrom_layout *layout = NULL;
	FmapAreaHeader *ah = NULL;

	fmap_find_by_name(NULL, 0, helper_fmap, "FMAP", &ah);
	if (!ah) {
		ERROR("Invalid helper fmap: no FMAP section.\n");
		goto end;
	}
	*fmap_len = ah->area_size;
	*fmap_pos = ah->area_offset;

	VB2_DEBUG("Looking for FMAP at %" PRIu32 " (%" PRIu32 " bytes)\n", ah->area_offset,
		  ah->area_size);

	if (flashrom_layout_read_fmap_from_rom(&layout, flashctx, ah->area_offset,
					       ah->area_size) != 0)
		goto end;

	flashrom_layout_include_region(layout, "FMAP");
	flashrom_layout_set(flashctx, layout);
	if (flashrom_image_read(flashctx, image->data, flash_len) != 0)
		goto end;

	image->fmap_header = fmap_find(image->data + ah->area_offset, ah->area_size);

end:
	VB2_DEBUG("Located FMAP using helper fmap: %s\n", image->fmap_header ? "YES" : "NO");
	flashrom_layout_release(layout);
}

enum flashrom_error {
	FLASHROM_SUCCESS = 0,
	FLASHROM_INIT_FAILURE = -1,
	FLASHROM_PROBE_FAILURE = -2,
	FLASHROM_GETSIZE_FAILURE = -3,
};

/**
 * Prepares flash for operations.
 *
 * `len` is set to the flash size. `programmer` and `params` are extracted into `tmp`, which has
 * to be released by the caller after flashrom programmer `prog` has been shut down.
 */
static enum flashrom_error flashrom_prepare_flash(struct flashrom_flashctx **flashctx,
						  size_t *len,
						  struct flashrom_programmer **prog, char **tmp,
						  const char *image_programmer,
						  char **programmer, char **params)
{
	/* Note: tmp stores programmer and params, so it has */
	*tmp = flashrom_extract_params(image_programmer, programmer, params);

	*prog = NULL;
	*flashctx = NULL;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);
	if (flashrom_init(1) || flashrom_programmer_init(prog, *programmer, *params))
		return FLASHROM_INIT_FAILURE;

	if (flashrom_flash_probe(flashctx, *prog, NULL))
		return FLASHROM_PROBE_FAILURE;

	if (len != NULL) {
		*len = flashrom_flash_getsize(*flashctx);
		if (!*len) {
			ERROR("Chip found had zero length, probing probably failed.\n");
			return FLASHROM_GETSIZE_FAILURE;
		}
	}
	return FLASHROM_SUCCESS;
}

/* Reads flash at given offsets */
int flashrom_read_segments(struct firmware_image *image, uint64_t offset[], size_t size[],
			   size_t segments_count, int verbosity)
{
	int r = -1;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;
	size_t len = 0;

	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	r = flashrom_prepare_flash(&flashctx, &len, &prog, &tmp, image->programmer,
				      &programmer, &params);

	switch (r) {
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
		goto err_probe;
	case FLASHROM_GETSIZE_FAILURE:
		goto err_cleanup;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	flashrom_layout_new(&layout);

	/* Regions must have unique string names. We will assign each region a string
	 * representation of consecutive numbers. */
	char region_name[FMAP_NAMELEN + 1];

	for (size_t i = 0; i < segments_count; i++) {
		snprintf(region_name, ARRAY_SIZE(region_name), "%zu", i);
		VB2_DEBUG("Including segment at %zu (size %" PRId64 ", %zu) ...\n", i,
			  offset[i], size[i]);

		if (size[i] == 0 || offset[i] + size[i] > len) {
			INFO("Invalid segment at %zu (size %" PRId64 ", %zu), ignoring.\n", i,
			     offset[i], size[i]);
			continue;
		}

		if (flashrom_layout_add_region(layout, offset[i], offset[i] + size[i] - 1,
					       region_name)) {
			INFO("Failed to add segment at %zu (size %" PRId64
			     ", %zu), ignoring.\n",
			     i, offset[i], size[i]);
			continue;
		}

		if (flashrom_layout_include_region(layout, region_name)) {
			INFO("Failed to include segment at %zu (size %" PRId64
			     ", %zu), ignoring.\n",
			     i, offset[i], size[i]);
			continue;
		}
	}

	flashrom_layout_set(flashctx, layout);

	r = flashrom_image_read(flashctx, image->data, len);

err_cleanup:
	flashrom_layout_release(layout);
	flashrom_flash_release(flashctx);

err_probe:
	r |= flashrom_programmer_shutdown(prog);

err_init:
	free(tmp);
	return r;
}

/**
 * NOTE: When `regions` contains multiple regions, `region_start` and
 * `region_len` will be filled with the data of the first region.
 *
 * If `helper_fmap` is provided, it will be used to locate
 * FMAP in flash. It that fails, FMAP will be located by searching the entire flash.
 */
static int flashrom_read_image_impl(struct firmware_image *image,
				    FmapHeader *helper_fmap,
				    const char *const regions[], const size_t regions_len,
				    unsigned int *region_start, unsigned int *region_len,
				    int verbosity)
{
	int r = -1;
	*region_start = 0;
	*region_len = 0;
	uint64_t fmap_pos = 0;
	size_t fmap_len = 0;
	FmapAreaHeader *ah;

	g_verbose_screen = (verbosity == -1) ? FLASHROM_MSG_INFO : verbosity;

	size_t len = 0;
	char *tmp, *programmer, *params;
	struct flashrom_programmer *prog = NULL;
	struct flashrom_flashctx *flashctx = NULL;
	struct flashrom_layout *layout = NULL;

	r = flashrom_prepare_flash(&flashctx, &len, &prog, &tmp, image->programmer,
				      &programmer, &params);
	switch (r) {
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
		goto err_probe;
	case FLASHROM_GETSIZE_FAILURE:
		goto err_cleanup;
	}

	flashrom_flag_set(flashctx, FLASHROM_FLAG_SKIP_UNREADABLE_REGIONS, true);

	if (!image->data) {
		image->data = calloc(1, len);
		if (!image->data) {
			ERROR("could not allocate image data (%zu bytes)\n", len);
			goto err_cleanup;
		}
		image->size = len;
		image->file_name = strdup("<sys-flash>");
		image->fmap_header = NULL;
	} else if (!image->fmap_header) {
		ERROR("Reading additional regions failed: missing FMAP.\n");
		goto err_cleanup;
	} else if (!fmap_find_by_name(image->data, image->size, image->fmap_header, "FMAP",
				      &ah)) {
		ERROR("Reading additional regions failed: did not find FMAP.");
		goto err_cleanup;
	} else {
		fmap_len = ah->area_size;
		fmap_pos = ah->area_offset;
	}

	if (regions_len) {
		/* Try locating FMAP by using the provided one. Note: if it fails,
		 * `image->fmap_header` will stay NULL. */
		if (helper_fmap && !image->fmap_header) {
			locate_fmap_using_helper_fmap(flashctx, image, helper_fmap, &fmap_pos,
						      &fmap_len, len);
		}

		/* If FMAP was located, or was already provided, read it into the layout. */
		if (image->fmap_header &&
		    flashrom_layout_read_fmap_from_buffer(
			    &layout, flashctx, image->data + fmap_pos, fmap_len) != 0) {
			VB2_DEBUG("Failed to locate FMAP using helper image. Will search the "
				  "flash...");
			image->fmap_header = NULL;
		}

		/* If FMAP was not found, or was not read into the layout correctly, fall
		 * back to the default searching mechanizm. */
		if (!image->fmap_header) {
			if (flashrom_layout_read_fmap_from_rom(&layout, flashctx, 0, len)) {
				ERROR("could not read fmap from rom, r=%d\n", r);
				goto err_cleanup;
			}

			/* Since we want to read at least one region and we did not find FMAP,
			 * `image->fmap_header` is still NULL and it will be later parsed from
			 * the image data. For this purpose we need to include the FMAP region.
			 */
			VB2_DEBUG("Including region 'FMAP' (because FMAP was not located)\n");
			if (flashrom_layout_include_region(layout, "FMAP")) {
				ERROR("could not include FMAP region\n");
				goto err_cleanup;
			}
		}
		int i;
		for (i = 0; i < regions_len; i++) {
			// empty region causes seg fault in API.
			r |= flashrom_layout_include_region(layout, regions[i]);
			if (r > 0) {
				ERROR("could not include region = '%s'\n",
				      regions[i]);
				goto err_cleanup;
			}
		}
		flashrom_layout_set(flashctx, layout);
	}

	r = flashrom_image_read(flashctx, image->data, len);

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
	return r;
}

int flashrom_read_image(struct firmware_image *image,
			FmapHeader *helper_fmap,
			const char * const regions[],
			const size_t regions_len,
			int verbosity)
{
	unsigned int start, len;
	return flashrom_read_image_impl(image, helper_fmap, regions, regions_len, &start,
					&len, verbosity);
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
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
		goto err_probe;
	case FLASHROM_GETSIZE_FAILURE:
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
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
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
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
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
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
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
	case FLASHROM_INIT_FAILURE:
		goto err_init;
	case FLASHROM_PROBE_FAILURE:
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
