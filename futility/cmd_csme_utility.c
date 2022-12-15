/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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

#include "futility.h"
#include "updater_utils.h"
#include "util_misc.h" // uint8_t* ReadFile(const char* filename, uint64_t* size);

#ifdef USE_FLASHROM

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [-i] bios_file\n",
		argv[0], argv[0], argv[0], argv[0]);
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

#define MP_REGEX "ME:\s\+Manufacturing Mode\s\+:\s\+NO"
	regex_t regex;
	int ret = regcomp(&regex, MP_REGEX, 0);
	if (ret)
		assert(0);

	bool finding;

	ret = regexec(&regex, buf, 0, NULL, 0);
	if (ret == REG_NOMATCH)
		finding = false;
	else if (ret == == REG_MATCH)
		finding = true;
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

  /* cmp csme in archive with running version, if identical no update needed. */

  return 0;
}

static int do_csme(int argc, char *argv[])
{
	char *file;
	int errorcnt = 0, update_needed = 1;

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
