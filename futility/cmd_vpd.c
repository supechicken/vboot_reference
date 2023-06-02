/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "flash_helpers.h"
#include "futility.h"
#include "updater.h"

#ifdef USE_FLASHROM

/* Command line options */
static struct option const long_opts[] = {
	SHARED_FLASH_ARGS_LONGOPTS
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{"debug", 0, NULL, 'd'},
	{"verbose", 0, NULL, 'v'},
	{NULL, 0, NULL, 0},
};

static const char *const short_opts = "hdv" SHARED_FLASH_ARGS_SHORTOPTS;

static void print_help(int argc, char *argv[])
{
	printf("\n"
	       "Usage:  " MYNAME " %s [OPTIONS]\n"
	       "\n"
	       "Reads VPD data from system firmware\n"
	       "-d, --debug         \tPrint debugging messages\n"
	       "-v, --verbose       \tPrint verbose messages\n"
	       SHARED_FLASH_ARGS_HELP,
	       argv[0]);
}

static char *vpd_get_list(struct updater_config *cfg, const char *fpath)
{
	char *command;
	ASPRINTF(&command, "/usr/sbin/vpd -l -f %s 2>/dev/null", fpath);
	char *result = host_shell(command);
	free(command);

	if (result && !*result) {
		free(result);
		result = NULL;
	}
	return result;
}

static int read_vpd_from_flash(struct updater_config *cfg)
{
	/* always need to read the FMAP to find regions. */
        const char *const regions[] = {
		FMAP_RO_FMAP,
		FMAP_RO_VPD, FMAP_RW_VPD,
		NULL
	};

	/* Read only the specified regions */
	if (flashrom_read_image(&cfg->image_current, regions,
				cfg->verbosity + 1)) {
		return -1;
	}

	const char *fpath = create_temp_file(&cfg->tempfiles);
	if (!fpath)
		return -1;

	if (write_to_file(NULL, fpath,
			  cfg->image_current.data,
			  cfg->image_current.size)) {
		return -1;
	}
	const char *s = vpd_get_list(cfg, fpath);
	if (!s) {
		ERROR("No valid VPD data found from flash.\n");
		return -1;
	}
	printf("%s\n", s);

	return 0;
}

static int do_vpd(int argc, char *argv[])
{
	struct updater_config *cfg = NULL;
	struct updater_config_arguments args = {0};
	int i, errorcnt = 0;

	opterr = 0;
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		if (handle_flash_argument(&args, i, optarg))
			continue;
		switch (i) {
		case 'h':
			print_help(argc, argv);
			return 0;
		case 'd':
			debugging_enabled = 1;
			args.verbosity++;
			break;
		case 'v':
			args.verbosity++;
			break;
		case '?':
			errorcnt++;
			if (optopt)
				ERROR("Unrecognized option: -%c\n", optopt);
			else if (argv[optind - 1])
				ERROR("Unrecognized option (possibly '%s')\n",
				      argv[optind - 1]);
			else
				ERROR("Unrecognized option.\n");
			break;
		default:
			errorcnt++;
			ERROR("Failed parsing options.\n");
		}
	}
	if (optind < argc) {
		ERROR("Unexpected arguments.\n");
		print_help(argc, argv);
		return 1;
	}

	if (setup_flash(&cfg, &args)) {
		ERROR("While preparing flash\n");
		return 1;
	}

	if (read_vpd_from_flash(cfg) < 0)
		errorcnt++;

	teardown_flash(cfg);
	return !!errorcnt;
}
#define CMD_HELP_STR "Read VPD data from system firmware"

#else /* USE_FLASHROM */

static int do_vpd(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `read` command unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Read VPD data from system firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(vpd, do_vpd, VBOOT_VERSION_ALL, CMD_HELP_STR);
