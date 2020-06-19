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

	memset(log, 0, sizeof(*log));

	/* Get textbox information. */
	vb2ex_get_textbox_size(&log->chars_per_line, &log->lines_per_page);
	if (log->chars_per_line <= 0 || log->lines_per_page <= 0)
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
	log->str_end = ptr;

	/* Count number of pages. */
	log->num_page = lines / log->lines_per_page;
	if (lines % log->lines_per_page != 0)
		log->num_page++;
	log->page_start = (const char **)malloc(log->num_page *
						sizeof(const char *));
	log->page_size = (size_t *)malloc(log->num_page * sizeof(size_t));
	if (!log->page_start || !log->page_size)
		return 0;

	/* Calculate the starting position of each page. */
	ptr = str;
	for (i = 0; i < log->num_page && *ptr != '\0'; i++) {
		/* TODO: Arrange the same amount of lines for the last page. */
		log->page_start[i] = ptr;
		lines = 0;
		while (*ptr != '\0' && lines < log->lines_per_page) {
			if (*ptr == '\n')
				lines++;
			ptr++;
		}
	}

	/* Calculate the size of each page. */

	for(i = 0; i < log->num_page; i++) {
		if (i == log->num_page - 1)
			log->page_size[i] = log->str_end - log->page_start[i];
		else
			log->page_size[i] = log->page_start[i + 1] -
					    log->page_start[i];
	}

	log->str = str;
	log->initialized = 1;
	log->previous_page = -1;

	VB2_DEBUG("Initialize logs: %d pages\n", log->num_page);

	return 1;
}

void log_final(struct vb2_ui_log_info *log)
{
	log->str = NULL;
	free(log->page_start);
	free(log->page_size);
	log->initialized = 0;
}

const char *log_get_current_page(struct vb2_ui_log_info *log)
{
	int cur_page;
	size_t page_size, buf_size;

	cur_page = log->current_page;
	buf_size = sizeof(log->buf);

	if (cur_page != log->previous_page) {
		VB2_DEBUG("Show page %d\n", cur_page);
		if (cur_page < 0 || cur_page >= log->num_page)
			return NULL;
		page_size = log->page_size[cur_page];
		if (page_size > buf_size - 1)
			page_size = buf_size - 1;
		strncpy(log->buf, log->page_start[cur_page], page_size);
		log->buf[page_size - 1] = '\0';
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
