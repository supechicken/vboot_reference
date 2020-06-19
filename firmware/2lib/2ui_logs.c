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

int log_init(struct vb2_ui_log_info *log, const char *str)
{
	int chars, lines;
	int i;
	const char *ptr;

	memset(log, 0, sizeof(log));

	/* Get textbox information. */
	vb2ex_get_textbox_size(&log->chars_per_line, &log->lines_per_page);
	if (log->chars_per_line <= 0 || log->lines_per_page <= 1)
		return 0;

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
	log->line_start = (const char **)malloc((lines + 1) *
						sizeof(const char *));
	if (!log->line_start)
		return 0;

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

	VB2_DEBUG("Initialize logs: %d pages\n", log->num_page);

	return 1;
}

void log_final(struct vb2_ui_log_info *log)
{
	log->str = NULL;
	free(log->line_start);
	log->initialized = 0;
}

const char *log_get_current_page(struct vb2_ui_log_info *log)
{
	int cur_page, page_size;

	cur_page = log->current_page;

	if (cur_page != log->previous_page) {
		VB2_DEBUG("Show page %d\n", cur_page);
		if (cur_page < 0 || cur_page >= log->num_page)
			return NULL;
		page_size = log->line_start[cur_page + 1] -
			    log->line_start[cur_page];
		strncpy(log->buf, log->line_start[cur_page], page_size);
		log->previous_page = cur_page;
		log->need_redraw = 1;
	}

	return log->buf;
}

void log_page_up(struct vb2_ui_log_info *log)
{
	log->previous_page = log->current_page;

	if (log->current_page > 0)
		log->current_page--;
}

void log_page_down(struct vb2_ui_log_info *log)
{
	log->previous_page = log->current_page;

	if (log->current_page < log->num_page - 1)
		log->current_page++;
}

int log_need_redraw(struct vb2_ui_log_info *log)
{
	if (log->initialized && log->need_redraw) {
		log->need_redraw = 0;
		return 1;
	}

	return 0;
}
