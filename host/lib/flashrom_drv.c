/* Copyright 2021 The Chromium OS Authors. All rights reserved.
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

struct flashrom_session {
	struct flashrom_programmer *prog;
	struct flashrom_flashctx *flashctx;
	struct flashrom_layout *layout;
	size_t len;
	char *prog_tokens;
	bool programmer_need_shutdown;
};

static int flashrom_end_session(struct flashrom_session *session)
{
	int r;

	if (session->layout)
		flashrom_layout_release(session->layout);
	if (session->flashctx)
		flashrom_flash_release(session->flashctx);
	if (session->programmer_need_shutdown)
		r = flashrom_programmer_shutdown(session->prog);
	free(session->prog_tokens);
	return r;
}

static int flashrom_include_regions(const struct flashrom_params * const params,
				    struct flashrom_session *session)
{
	int i, r;

	r = flashrom_layout_read_fmap_from_buffer(
			&session->layout, session->flashctx,
			params->image->data, params->image->size);
	if (r > 0) {
		WARN("Could not read FMAP from image, r=%d, "
		     "falling back to read from ROM\n", r);
		r = flashrom_layout_read_fmap_from_rom(
			&session->layout, session->flashctx, 0, session->len);

		if (r > 0) {
			ERROR("could not read FMAP from ROM, r=%d\n", r);
			return -1;
		}
	}

	// Empty region causes seg fault in API.
	for (i = 0; params->regions[i]; i++) {
		// empty region causes seg fault in API.
		r |= flashrom_layout_include_region(
				session->layout, params->regions[i]);
		if (r > 0) {
			ERROR("Could not include region = '%s', r=%d\n",
			      params->regions[i], r);
			return -1;
		}
		VB2_DEBUG("Include new region: %s\n", params->regions[i]);
	}
	flashrom_layout_set(session->flashctx, session->layout);
	return 0;
}

static int flashrom_start_session(const struct flashrom_params * const params,
				  struct flashrom_session *session)
{
	char *prog_name, *prog_params;

	if (params->verbose == -1)
		g_verbose_screen = FLASHROM_MSG_INFO;
	else
		g_verbose_screen = params->verbose;
	flashrom_set_log_callback((flashrom_log_callback *)&flashrom_print_cb);

	session->prog_tokens = flashrom_extract_params(
			params->image->programmer, &prog_name, &prog_params);

	if (flashrom_init(1) ||
	    flashrom_programmer_init(&session->prog, prog_name, prog_params)) {
		ERROR("Failed initializing programmer: %s\n", prog_name);
		return -1;
	}
	/* In current implementation, session->prog is always NULL even if the
	 * programmer has been initialized, so we need an explicit flag. */
	session->programmer_need_shutdown = true;


	if (flashrom_flash_probe(&session->flashctx, session->prog, NULL)) {
		ERROR("Failed probing flash chip.\n");
		return -1;
	}

	session->len = flashrom_flash_getsize(session->flashctx);

	if (params->flash_contents &&
	    params->flash_contents->size != params->image->size) {
		ERROR("flash_contents->size != image->size");
		return -1;
	}

	if (params->regions && flashrom_include_regions(params, session)) {
		ERROR("Failed setting regions to include.\n");
		return -1;
	}

	/* Set flags */
	flashrom_flag_set(session->flashctx, FLASHROM_FLAG_FORCE,
			  params->force);
	flashrom_flag_set(session->flashctx, FLASHROM_FLAG_VERIFY_AFTER_WRITE,
			  !params->noverify);
	flashrom_flag_set(session->flashctx, FLASHROM_FLAG_VERIFY_WHOLE_CHIP,
			  !params->noverify_all);

	return 0;
}

int flashrom_read_image(const struct flashrom_params * const params)
{
	int r = 0;
	struct flashrom_session session = {0};

	if (flashrom_start_session(params, &session)) {
		r = -1;
		goto err_cleanup;
	}

	params->image->data = calloc(1, session.len);
	params->image->size = session.len;
	params->image->file_name = strdup("<sys-flash>");

	r |= flashrom_image_read(session.flashctx, params->image->data,
				 session.len);

err_cleanup:
	r |= flashrom_end_session(&session);
	return r;
}

int flashrom_write_image(const struct flashrom_params * const params)
{
	int r = 0;
	struct flashrom_session session = {0};

	if (flashrom_start_session(params, &session)) {
		r = -1;
		goto err_cleanup;
	}

	if (session.len == 0) {
		ERROR("zero sized flash detected\n");
		r = -1;
		goto err_cleanup;
	}

	r |= flashrom_image_write(session.flashctx, params->image->data,
				  params->image->size, params->flash_contents ?
				  params->flash_contents->data : NULL);
err_cleanup:
	r |= flashrom_end_session(&session);
	return r;
}
