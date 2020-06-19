/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware screen pagination tools for logs.
 */

#include "2api.h"
#include "2common.h"
#include "2ui.h"
#include "2ui_private.h"

vb2_error_t log_init(struct vb2_ui_log_info *log, const char *str)
{
	int chars, lines;
	int i;
	const char *ptr;

	if (log == NULL)
		return VB2_ERROR_UI_LOG_INFO;

	memset(log, 0, sizeof(log));

	/* Get textbox information. */
	vb2ex_get_textbox_size(&log->chars_per_line, &log->lines_per_page);
	if (log->chars_per_line <= 0 || log->lines_per_page <= 1)
		return VB2_ERROR_UI_LOG_INFO;

	/* Count number of characters and lines. */
	chars = 0;
	lines = 0;
	ptr = str;
	while (*ptr != '\0') {
		if (*ptr == '\n')
			lines++;
		chars++;
		ptr++;
	}

	/* Count number of pages. */
	/* TODO: Repeat the last line in previous page. */
	log->num_page = lines / log->lines_per_page;
	if (lines % log->lines_per_page != 0)
		log->num_page++;

	/* Calculate the positions of starting characters each line. */
	ptr = str;
	for (i = 0; i < log->num_page && *ptr != '\0'; i++) {
		/* TODO: Arrange the same amount of lines for the last page. */
		log->line_start[i] = ptr;
		lines = 0;
		while (*ptr != '\0' && lines < log->lines_per_page) {
			if (*ptr == '\n')
				lines++;
			ptr++;
		}
	}
	log->line_start[log->num_page] = str + chars;

	log->str = str;
	log->initialized = 1;

	/* Debug */
	VB2_DEBUG("Initialize logs: %d pages\n", log->num_page);
	for (i = 0; i < log->num_page; i++) {
		log->current_page = i;
		VB2_DEBUG(">> Page %d:\n%s\n", i, log_get_current_page(log));
	}
	log->current_page = 0;

	return VB2_SUCCESS;
}

vb2_error_t log_final(struct vb2_ui_log_info *log)
{
	if (log == NULL)
		return VB2_ERROR_UI_LOG_INFO;

	log->str = NULL;
	log->initialized = 0;

	return VB2_SUCCESS;
}

const char *log_get_current_page(struct vb2_ui_log_info *log)
{
	int cur_page, page_size;

	if (log == NULL || !log->initialized)
		return NULL;

	VB2_DEBUG("Show page %d\n", log->current_page);

	cur_page = log->current_page;
	if (cur_page < 0|| cur_page >= log->num_page)
		return NULL;

	page_size = log->line_start[cur_page+1] - log->line_start[cur_page];

	strncpy(log->buf, log->line_start[cur_page], page_size);
	return log->buf;
}

vb2_error_t log_page_up(struct vb2_ui_log_info *log)
{
	if (log == NULL || !log->initialized)
		return VB2_ERROR_UI_LOG_INFO;

	log->previous_page = log->current_page;

	if (log->current_page > 0)
		log->current_page--;

	return VB2_SUCCESS;
}

vb2_error_t log_page_down(struct vb2_ui_log_info *log)
{
	if (log == NULL || !log->initialized)
		return VB2_ERROR_UI_LOG_INFO;

	log->previous_page = log->current_page;

	if (log->current_page < log->num_page - 1)
		log->current_page++;

	return VB2_SUCCESS;
}

int log_changed(struct vb2_ui_log_info *log)
{
	if (log == NULL || !log->initialized)
		return 0;

	if (log->previous_page != log->current_page) {
		log->previous_page = log->current_page;
		return 1;
	}

	return 0;
}
