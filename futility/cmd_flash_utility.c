/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>
#include <fcntl.h>

#include "futility.h"
#include "updater.h"
#include "updater_utils.h"

#ifdef USE_FLASHROM

static void print_help(int argc, char *argv[])
{
	printf("\nUsage:  " MYNAME " %s\n", argv[0]);
}

/* Command line options */
static struct option long_opts[] = {
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},

	{NULL, 0, NULL, 0},
};
static const char *short_opts = "h";

/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

static int do_flashinfo(int argc, char *argv[])
{
	int errorcnt = 0, update_needed = 1;
	unsigned int i;

	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'h':
			print_help(argc, argv);
			return !!errorcnt;
		case '?':
			errorcnt++;
			if (optopt)
				ERROR("unrecognized option: -%c\n", optopt);
			else if (argv[optind - 1])
				ERROR("unrecognized option (possibly \"%s\")\n",
					argv[optind - 1]);
			else
				ERROR("unrecognized option\n");
			break;
		default:
			errorcnt++;
			ERROR("error while parsing options\n");
		}
	}

	/* Problems? */
	if (errorcnt) {
		print_help(argc, argv);
		return 1;
	}

	struct updater_config_arguments args = {0};
	struct updater_config *cfg = updater_new_config();
	assert(cfg);

	if (!errorcnt)
		errorcnt += updater_setup_config(cfg, &args, &update_needed);
	if (errorcnt || !update_needed) {
		goto cleanups;
	}

	const struct firmware_image *image = &cfg->image;
	char *flash_name;
	uint32_t flash_size;
	if (!flashrom_flash_info(image->programmer, &flash_name, &flash_size, -1)) {
		INFO("flash size = 0x%x\n", flash_size);
	}

cleanups:
	updater_delete_config(cfg);
	return !!errorcnt;
}
#define CMD_HELP_STR "SPI flash information"

#else /* USE_FLASHROM */

static int do_flashinfo(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `flashinfo` subcommand "
		     "unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Update CSME firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(flashinfo, do_flashinfo, VBOOT_VERSION_ALL, CMD_HELP_STR);
