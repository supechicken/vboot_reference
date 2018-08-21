/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * A reference implementation for AP (and supporting images) firmware updater.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "futility.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

struct updater_config {
};

enum updater_error_codes {
	UPDATE_ERR_NONE,
	UPDATE_ERR_UNKNOWN,
};

static const char *updater_error_messages[] = {
	[UPDATE_ERR_NONE] = "None",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_firmware(struct updater_config *cfg)
{
	return UPDATE_ERR_NONE;
}

/* Command line options */
static struct option long_opts[] = {
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static const char *short_opts = "";
static int errorcnt = 0;

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [OPTIONS]\n"
		"\n"
		"",
		argv[0]);
}

static int do_update(int argc, char *argv[])
{
	int i;
	struct updater_config cfg = {};

	opterr = 0;		/* quiet, you */
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'h':
			print_help(argc, argv);
			return !!errorcnt;

		case '?':
			errorcnt++;
			if (optopt)
				Error("Unrecognized option: -%c\n", optopt);
			else if (argv[optind - 1])
				Error("Unrecognized option (possibly '%s')\n",
				      argv[optind - 1]);
			else
				Error("Unrecognized option.\n");
			break;
		default:
			errorcnt++;
			Error("Failed parsing options.\n");
		}
	}

	if (!errorcnt) {
		int r = update_firmware(&cfg);
		errorcnt += r;
		if (r == UPDATE_ERR_NONE) {
			printf("SUCCESS: Updater finished successfully.\n");
		} else {
			r = MIN(r, UPDATE_ERR_UNKNOWN);
			Error("%s\n", updater_error_messages[r]);
		}
	}

	return !!errorcnt;
}

DECLARE_FUTIL_COMMAND(update, do_update, VBOOT_VERSION_ALL,
		      "Update system firmware");
