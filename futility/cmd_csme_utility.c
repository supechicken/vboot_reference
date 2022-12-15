/* Copyright 2022 The ChromiumOS Authors
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
#include "host_misc.h" // uint8_t* ReadFile(const char* filename, uint64_t* size);

#ifdef USE_FLASHROM

static void print_help(int argc, char *argv[])
{
	printf("\nUsage:  " MYNAME " %s [-i] bios_file\n", argv[0]);
}

/* Command line options */
static struct option long_opts[] = {
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{"debug", 0, NULL, 'd'},
	{"verbose", 0, NULL, 'v'},

	{"image", 1, NULL, 'i'},
	{NULL, 0, NULL, 0},
};

static const char *short_opts = ":i:";

/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

static bool is_csme_locked(void)
{
	return false;
}

static bool is_csme_mp_mode(void)
{
	uint64_t sz;
	uint8_t *buf = ReadFile("/sys/firmware/log", &sz);
	if (!buf)
		assert(0);

	const char *mp_regex = "ME:\\s\\+Manufacturing Mode\\s\\+:\\s\\+NO";
	regex_t regex;
	int ret = regcomp(&regex, mp_regex, 0);
	if (ret)
		assert(0);

	bool finding;

	ret = regexec(&regex, (const char *)buf, 0, NULL, 0);
	if (!ret)
		finding = true;
	else if (ret == REG_NOMATCH)
		finding = false;
	else {
		char msgbuf[100];
		regerror(ret, &regex, msgbuf, sizeof(msgbuf));
		fprintf(stderr, "%s: match failed: %s\n", __func__, msgbuf);
		finding = false;
	}

	free(buf);
	return finding;
}

static int check_csme_update_possible(struct updater_config *cfg)
{

  // Returns true if wp is enabled on current system.
  if (is_write_protection_enabled(cfg))
    return -1; /* cannot update csme. */

  /* is csme locked? if so no update needed. */
  if (is_csme_locked())
    return -1;

  /* is csme mp mode? if so no update needed. */
  if (is_csme_mp_mode())
    return -1;

  /* cmp csme ro versions, if identical no update needed. */
  if (!section_needs_update(&cfg->image_current, &cfg->image, FMAP_SI_ME))
    return -1;

  /* cmp csme in archive with running version, if identical no update needed. */

  return 0;
}

/* Generate boot alert regarding an important update. */
static void generate_boot_alert(void)
{
	(void) system("chromeos-boot-alert update_csme_firmware");
}

/**
 * Request a cold boot by producing the below file which is consumed by
 * boot_update_firmware.conf upstart script that invoked this script.
 */
static void request_cold_reboot(void)
{
	const char *path = "/tmp/force_cold_boot_after_fw_update";
	close(open(path, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH));
}

/* Flash the ME region using the BIOS extracted from archive. */
static enum updater_error_codes flash_csme_update(struct updater_config *cfg)
{
//flashrom -p host -i SI_ME -w "${ARCHIVE_BIOS}" >/dev/null
	struct firmware_image *image_to = &cfg->image;
	if (write_firmware(cfg, image_to, FMAP_SI_ME))
		return UPDATE_ERR_WRITE_FIRMWARE;

	return UPDATE_ERR_DONE;
}

static int do_csme(int argc, char *argv[])
{
	char *file;
	int errorcnt = 0, update_needed = 1;
	unsigned int i;

	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'i':
			file = optarg;
			break;
		case 'h':
			print_help(argc, argv);
			return !!errorcnt;
		case '?':
			errorcnt++;
			if (optopt)
				fprintf(stderr,
					"ERROR: unrecognized option: -%c\n",
					optopt);
			else if (argv[optind - 1])
				fprintf(stderr,
					"ERROR: unrecognized option "
					"(possibly \"%s\")\n",
					argv[optind - 1]);
			else
				fprintf(stderr, "ERROR: unrecognized option\n");
			break;
		default:
			errorcnt++;
			fprintf(stderr,
				"ERROR: error while parsing options\n");
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

	if (check_csme_update_possible(cfg) < 0) {
		fprintf(stderr, "NO CSME updates for you!\n");
	} else {
		STATUS("Starting CSME firmware update.\n");
		generate_boot_alert();
		flash_csme_update(cfg);
		request_cold_reboot();
	}

cleanups:
	updater_delete_config(cfg);
	return !!errorcnt;
}
#define CMD_HELP_STR "Update CSME firmware"

#else /* USE_FLASHROM */

static int do_csme(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `csme` subcommand unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Update CSME firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(csme, do_csme, VBOOT_VERSION_ALL, CMD_HELP_STR);
