/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include "call_trace.h"

#ifdef VB_TRACE_CALL

static struct call_trace *call_trace = NULL;

void vb_init_call_trace(struct call_trace *ct)
{
	if (!ct)
		return;
	call_trace = ct;
	/*
	 * TODO: If this is used in a firmware lib, this should be expanded to
	 * a for loop or replaced by safe_memset.
	 */
	memset(call_trace, 0, sizeof(*call_trace));
}

int vb_push_return_code(const char *func, int err)
{
	if (!call_trace)
		return err;

	if (err || VB_TRACE_SUCCESS) {
		unsigned int idx = call_trace->idx % VB_NUM_CALL_RECORD;
		call_trace->rec[idx].err = err;
		call_trace->rec[idx].func = func;
		call_trace->idx++;
	}

	return err;
}

void vb_dump_call_trace(void)
{
	int i;

	if (!call_trace) {
		fprintf(stderr, "Invalid call trace pointer\n");
		return;
	}

	fprintf(stderr,
		"CALL TRACE (older first, index=%d)\n", call_trace->idx);
	for (i = 0; i < VB_NUM_CALL_RECORD && i < call_trace->idx; i++)
		fprintf(stderr, "  %s:%08x\n",
			call_trace->rec[i].func, call_trace->rec[i].err);
}

#else

void vb_init_call_trace(struct call_trace *ct) {}
int vb_push_return_code(const char *func, int err) {return err;}
void vb_dump_call_trace(void) {}

#endif
