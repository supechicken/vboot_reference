/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "fmap.h"
#include "futility.h"
#include "updater.h"

#ifdef USE_FLASHROM

static int get_ro_range(struct updater_config *cfg, size_t *start, size_t *len)
{
	int ret = 0;

	/* Read fmap */
	const char *const regions[] = {FMAP_RO_FMAP, NULL};
	if (flashrom_read_image(&cfg->image_current, regions, cfg->verbosity + 1))
		return 1;

	FmapAreaHeader *wp_ro;
	if (!fmap_find_by_name(cfg->image_current.data, cfg->image_current.size, NULL, "WP_RO", &wp_ro)) {
		ERROR("Could not find WP_RO in the FMAP\n");
		ret = 1;
		goto err;
	}

	*start = wp_ro->area_offset;
	*len = wp_ro->area_size;

err:
	free(cfg->image_current.data);
	cfg->image_current.data = NULL;
	cfg->image_current.size = 0;

	return ret;
}

static int print_wp_status(struct updater_config *cfg)
{
	size_t ro_start, ro_len;
	int ret = get_ro_range(cfg, &ro_start, &ro_len);
	if (ret)
		return ret;

	bool wp_mode;
	size_t wp_start, wp_len;
	ret = flashrom_get_wp(cfg->image.programmer, &wp_mode,
			      &wp_start, &wp_len, -1);
	if (ret) {
		ERROR("Failed to get WP status\n");
		return 1;
	}

	if (!wp_mode && wp_start == 0 && wp_len == 0) {
		printf("WP status: disabled\n");
	} else if (wp_mode && wp_start == ro_start && wp_len == ro_len) {
		printf("WP status: enabled\n");
	} else {
		printf("WP status: misconfigured\n");
	}

	return 0;
}

/*
static int print_flash_size(struct updater_config *cfg)
{
	const struct firmware_image *image = &cfg->image;
	char *flash_name;
	uint32_t flash_size;
	if (!flashrom_flash_info(image->programmer, &flash_name, &flash_size, -1)) {
		INFO("flash size = 0x%x\n", flash_size);
	}
	return 0;
}
*/


/* Command line options */
static struct option const long_opts[] = {
	SHARED_FLASH_ARGS_LONGOPTS
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{"wp-status", 0, NULL, 's'},
	{"wp-enable", 0, NULL, 'e'},
	{"wp-disable", 0, NULL, 'd'},
	{"size", 0, NULL, 'z'},
	{NULL, 0, NULL, 0},
};

static const char *const short_opts = "h" SHARED_FLASH_ARGS_SHORTOPTS;

static void print_help(int argc, char *argv[])
{
	printf("\n"
	       "Usage:  " MYNAME " %s [OPTIONS] \n"
	       "\n"
	       "    --wp-status      \tGet the current flash WP state.\n"
	       "    --wp-enable      \tEnable protection for the RO image section.\n"
	       "    --wp-disable     \tDisable all write protection.\n"
	       "    --flash-size     \tGet flash size.\n"
	       "\n"
	       SHARED_FLASH_ARGS_HELP,
	       argv[0]);
}

static int do_flash(int argc, char *argv[])
{
	int ret = 0;
	struct updater_config_arguments args = {0};
	const char *prepare_ctrl_name = NULL;
	char *servo_programmer = NULL;
	int enable_wp = 0;
	int disable_wp = 0;
	int get_wp_status = 0;
	int get_size = 0;

	struct updater_config *cfg = updater_new_config();
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
		case 'z':
			get_size = 1;
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

	if (enable_wp && disable_wp) {
		ret = 1;
		ERROR("--wp-enable and --wp-disable cannot be used together.\n");
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

	int update_needed;
	ret = updater_setup_config(cfg, &args, &update_needed);
	if (ret)
		goto err;

	if (get_wp_status) {
		if (print_wp_status(cfg)) {
			ret = 1;
			goto err;
		}
	}

err:
	free(servo_programmer);
	updater_delete_config(cfg);
	return ret;
}
#define CMD_HELP_STR "Manipulate AP SPI flash"

#else /* USE_FLASHROM */

static int do_flash(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `flash` command unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Manipulate AP SPI flash (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(flash, do_flash, VBOOT_VERSION_ALL, CMD_HELP_STR);
