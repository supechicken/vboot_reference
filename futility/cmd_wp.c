/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "futility.h"
#include "updater.h"

#ifdef USE_FLASHROM

/* Command line options */
static struct option const long_opts[] = {
	SHARED_FLASH_ARGS_LONGOPTS
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{"status", 0, NULL, 'g'},
	{"enable", 0, NULL, 'e'},
	{"disable", 0, NULL, 'd'},
	{NULL, 0, NULL, 0},
};

static const char *const short_opts = "hsed" SHARED_FLASH_ARGS_SHORTOPTS;

static void print_help(int argc, char *argv[])
{
	printf("\n"
	       "Usage:  " MYNAME " %s [OPTIONS] \n"
	       "\n"
	       "-s, --status (default) \tGet the current flash WP state.\n"
	       "-e, --enable           \tEnable protection for the RO image section.\n"
	       "-d, --disable          \tDisable all write protection.\n"
	       "\n"
	       SHARED_FLASH_ARGS_HELP,
	       argv[0]);
}


/* WIP

#define WP_RO_REGION "WP_RO"
static int get_ro_section_range(size_t *start, size_t *len)
{
	struct firmware_section section;
	if (find_firmware_section(&section, &cfg->image_current,
				  WP_RO_REGION)) {
		ERROR("Region '%s' not found in image.\n", WP_RO_REGION);
		return -1;
	}

	return 0;
}
*/

static int print_wp_status(struct updater_config_arguments *args)
{
	// WIP. this currently only gets the WP mode, the WP range is ignored.
	// Ideally we should get the range and compare it against the proper RO
	// range as follows:
	//
	// (mode == disabled, range = 0,0)                  => "disabled"
	// (mode == hardware, range = expected RO range)    => "enabled"
	//
	// anything else => "misconfigured". I.e.
	// (mode == disabled, range = not 0,0)              => "misconfigured"
	// (mode == hardware, range = not expected RO range) => "misconfigured"
	//
	enum wp_state state = flashrom_get_wp(args->programmer, -1);
	switch (state) {
	case WP_ERROR:
		printf("Failed to get WP status\n");
		return -1;
	case WP_DISABLED:
		printf("WP status: disabled\n");
		break;
	case WP_ENABLED:
		printf("WP status: enabled\n");
		break;
	}

	return 0;
}

static int do_wp(int argc, char *argv[])
{
	int ret = 0;
	struct updater_config *cfg;
	struct updater_config_arguments args = {0};
	const char *prepare_ctrl_name = NULL;
	char *servo_programmer = NULL;
	int enable_wp = 0;
	int disable_wp = 0;
	int get_wp_status = 0;

	cfg = updater_new_config();
	assert(cfg);

	opterr = 0;
	int i;
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		if (handle_flash_argument(&args, i, optarg))
			continue;
		switch (i) {
		case 'h':
			print_help(argc, argv);
			updater_delete_config(cfg);
			return ret;
		case 's':
			get_wp_status = 1;
			break;
		case 'e':
			enable_wp = 1;
			break;
		case 'd':
			disable_wp = 1;
			break;
		case 'v':
			args.verbosity++;
			break;
		case '?':
			ret = 1;
			if (optopt)
				ERROR("Unrecognized option: -%c\n", optopt);
			else if (argv[optind - 1])
				ERROR("Unrecognized option (possibly '%s')\n",
				      argv[optind - 1]);
			else
				ERROR("Unrecognized option.\n");
			break;
		default:
			ret = 1;
			ERROR("Failed parsing options.\n");
		}
	}
	if (optind < argc) {
		ret = 1;
		ERROR("Unexpected arguments.\n");
	}

	if (!enable_wp && !disable_wp)
		get_wp_status = 1;

	if ((enable_wp + disable_wp + get_wp_status) > 1) {
		ret = 1;
		ERROR("Multiple -s/-e/-d options cannot be used together.\n");
		goto err;
	}

	if (args.detect_servo) {
		servo_programmer = host_detect_servo(&prepare_ctrl_name);

		if (!servo_programmer) {
			ret = 1;
			ERROR("No servo detected.\n");
			goto err;
		}
		if (!args.programmer)
			args.programmer = servo_programmer;
	}

	if (args.programmer == NULL) {
		ret = 1;
		ERROR("No programmer specified.\n");
		goto err;
	}

	int update_needed = 1;
	ret = updater_setup_config(cfg, &args, &update_needed);
	if (ret)
		goto err;

	if (get_wp_status) {
		if (print_wp_status(&args)) {
			ret = 1;
			goto err;
		}
	}

err:
	free(servo_programmer);
	updater_delete_config(cfg);
	return ret;
}
#define CMD_HELP_STR "Manipulate AP flash write protection"

#else /* USE_FLASHROM */

static int do_wp(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `wp` command unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Manipulate AP flash write protection (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(wp, do_wp, VBOOT_VERSION_ALL, CMD_HELP_STR);
