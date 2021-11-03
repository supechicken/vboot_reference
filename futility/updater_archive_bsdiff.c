/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Utility functions for handling delta files in the firmware updater.
 */

#include <string.h>

#include "futility.h"
#include "updater.h"
#include "updater_utils.h"

/* Theory of operation:
 *
 * A delta entry for foo consists of two files:
 * 1. foo-from-reference
 * 2. reference
 *
 * The name of the reference file can differ but through this scheme
 * the creator of the archive can spend as much effort as wanted or
 * needed, potentially using the optimal reference for each derived
 * file across the entire archive.
 *
 * To keep things simple, references may not themselves be delta
 * files.
 *
 * Example:
 *
 * images/image.kohaku.bin-from-image.hatch.bin
 * images/image.hatch.bin
 */

int bspatch_mem(const uint8_t *old_data, size_t old_size,
		const uint8_t *patch_data, size_t patch_size,
		uint8_t **out_data, size_t *out_size);

struct search_for_delta_arg {
	/* Inputs */
	struct archive *ar;
	const char *filename;
	/* Output */
	char *reference;
};

/* When given path "foo", look for "foo-from-bar" for existing file "bar".
 *
 * To be used as callback function in archive_walk.
 * It interprets arg as pointer to struct search_for_delta_arg.
 *
 * On success it returns the reference's file name in arg->reference in newly
 * allocated memory, otherwise arg->reference is left unchanged.
 * Caller must free arg->reference when done.
 */

static int search_for_delta(const char *path, void *arg)
{
	struct search_for_delta_arg *args = arg;

	int filenamelen = strlen(args->filename);
	if (strncmp(path, args->filename, filenamelen) != 0) {
		/* No match, look further. */
		return 0;
	}
	if (strncmp(path + filenamelen, "-from-", strlen("-from-")) != 0) {
		/* No match, look further. */
		return 0;
	}

	char *refbasename = (char *)path + filenamelen + strlen("-from-");
	int refbasenamelen = strlen(refbasename);
	if (refbasenamelen == 0) {
		/* This is "${filename}-from-", then nothing? Look further. */
		return 0;
	}

	char *basename = simple_basename((char *)path);
	char *refname;
	if (basename != path) {
		int dirnamelen = basename - path;
		/* Prepend dirname(path) */
		int refnamelen = dirnamelen + refbasenamelen + 1;
		refname = malloc(refnamelen);
		snprintf(refname, refnamelen,
			 "%.*s%s", dirnamelen, path, refbasename);
	} else {
		refname = strdup(refbasename);
	}

	if (!archive_has_entry(args->ar, refname)) {
		/* Reference file not found, look further. */
		free(refname);
		return 0;
	}

	/* Return the match */
	args->reference = refname;
	return 1;
}

int archive_has_delta_entry(struct archive *ar, const char *name)
{
	struct search_for_delta_arg arg = {
		.ar = ar,
		.filename = name,
		.reference = NULL,
	};
	archive_walk(ar, &arg, search_for_delta);

	int found = arg.reference != NULL;
	free(arg.reference);
	return found;
}

/*
 * Reads a file from archive.
 * If entry name (fname) is an absolute path (/file), always read
 * from real file system.
 * Returns 0 on success (data and size reflects the file content),
 * otherwise non-zero as failure.
 */
int archive_read_delta_file(struct archive *ar, const char *fname,
		      uint8_t **data, uint32_t *size, int64_t *mtime)
{
	struct search_for_delta_arg arg = {
		.ar = ar,
		.filename = fname,
		.reference = NULL,
	};
	archive_walk(ar, &arg, search_for_delta);

	if (arg.reference == NULL)
		return VB2_ERROR_UNKNOWN;

	uint8_t *reference_data;
	uint32_t reference_size;

	if (archive_read_file(ar, arg.reference,
	    &reference_data, &reference_size, NULL) != 0) {
		free(arg.reference);
		return VB2_ERROR_UNKNOWN;
	}

	/* Potentially too much, not a problem though. */
	char *referencebase = simple_basename((char *)arg.reference);
	char *delta_filename = malloc(strlen(fname) +
				      strlen("-from-") +
				      strlen(referencebase) +
				      1);
	sprintf(delta_filename, "%s-from-%s", fname, referencebase);
	free(arg.reference);
	arg.reference = NULL;

	uint8_t *delta_data;
	uint32_t delta_size;

	if (archive_read_file(ar, delta_filename,
	    &delta_data, &delta_size, NULL) != 0) {
		free(delta_filename);
		return VB2_ERROR_UNKNOWN;
	}
	free(delta_filename);
	delta_filename = NULL;

	size_t data_size;
	int ret = bspatch_mem(reference_data, reference_size,
			      delta_data, delta_size,
			      data, &data_size);

	/* TODO: boundary check */
	*size = (uint32_t)data_size;

	free(reference_data);
	free(delta_data);
	if (ret != 0) {
		free(*data);
		return ret;
	}

	return 0;
}
