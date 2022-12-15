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

/**
 * This function has been specifically crafted to deal with how
 * sysfs files are pinned pages. While they claim S_IFRAG in
 * st_mode whereas they more are like a FIFO (empty file).
 */
static char* read_sysfs_file_into_buf(const char* filename)
{
	char *buf = NULL;
	FILE* f = fopen(filename, "r");
	if (!f) {
		ERROR("Unable to open file %s\n", filename);
		return NULL;
	}

	/* determine the size of a sysfs file. */
	size_t size = 0;
	int c;
	while ((c = fgetc(f)) != EOF)
		size++;
	rewind(f);
	if (!size)
		goto err;

	buf = calloc(1, size);
	if (!buf)
		goto err;

	if(1 != fread(buf, size, 1, f)) {
		ERROR("Unable to read from file %s\n", filename);
		goto err;
	}

	fclose(f);
	return buf;

err:
	fclose(f);
	free(buf);
	return NULL;
}

static bool is_csme_locked(struct updater_config *cfg)
{
	if (load_system_firmware(cfg, &cfg->image_current)) {
		WARN("Could not load firmware image.\n");
		return true;
	}

	// Extract the SI_DESC region
	struct firmware_section si_desc;
	if (find_firmware_section(&si_desc, &cfg->image_current, FMAP_SI_DESC)) {
		WARN("Could not find SI_DESC region. Try in the next boot.\n");
		return true;
	}
	if (si_desc.size == 0) {
		ERROR("SI_DESC region is zero sized.\n");
		return true;
	}

	// Check if the value at offset 0x10 is FLVALSIG 0x0ff0a55a.
	const uint32_t flvalsig = si_desc.data[0x10] | (si_desc.data[0x11] << 8)
			| (si_desc.data[0x12] << 16) | (si_desc.data[0x13] << 24);
	if (flvalsig != 0x0ff0a55a) {
		INFO("Valid Flash Descriptor signature not found. ME is locked. \n");
		return true;
	}

	// Extract the flash master base address at 0x18 and use flash master base address as
	// an offset to extract flash master record. Check if bit 22 is set in flash master record.
	const uint8_t fmba = si_desc.data[0x18] << 4;
	const uint32_t flmstr = si_desc.data[fmba]     | (si_desc.data[fmba+1] << 8)
			| (si_desc.data[fmba+2] << 16) | (si_desc.data[fmba+3] << 24);
	INFO("%s(): debug - flmstr=0x%x\n", __func__, flmstr);
	if (( flmstr & (1 << 22) ) == 0) {
		INFO("Host CPU does not have write access to ME. ME is locked.\n");
		return true;
	}

	return false;
}

/* MP mode means "ME: Manufacturing Mode : NO" appears in log.
 * Hence, return 1 on a match, 0 on no match and -1 on error.
 */
static int is_csme_mp_mode(void)
{
	int finding = -1;

	char *buf = read_sysfs_file_into_buf("/sys/firmware/log");
	if (!buf)
		return -1;

	const char *mp_regex = "ME:\\s\\+Manufacturing Mode\\s\\+:\\s\\+NO";
	regex_t regex;
	int ret = regcomp(&regex, mp_regex, 0);
	if (ret)
		goto err;

	ret = regexec(&regex, (const char *)buf, 0, NULL, 0);
	if (!ret)
		finding = 1; /* match. */
	else if (ret == REG_NOMATCH)
		finding = 0; /* no match. */
	else {
		char msgbuf[100];
		regerror(ret, &regex, msgbuf, sizeof(msgbuf));
		WARN("%s: match failed: %s\n", __func__, msgbuf);
	}

err:
	free(buf);
	return finding;
}

static int check_csme_update_possible(struct updater_config *cfg)
{
	/* Is CSME in MP mode? if so no update needed. */
	int ret = is_csme_mp_mode();
	if (ret == 1) { /* match */
		STATUS("[x] - CSME is in MP mode.\n");
		return -1;
	} else if (ret < 0) /* no match or error. */
		return ret;
	STATUS("[✓] - CSME is not in MP mode.\n");

	/* Returns true if wp is enabled on current system. */
	if (is_write_protection_enabled(cfg)) {
		STATUS("[x] - Write Protection is on.\n");
		return -1; /* cannot update csme. */
	}
	STATUS("[✓] - Write Protection is off.\n");

	/* is csme locked? if so no update needed. */
	if (is_csme_locked(cfg)) {
		STATUS("[x] - CSME is locked.\n");
		return -1;
	}
	STATUS("[✓] - CSME is unlocked.\n");

	/* cmp csme ro versions, if identical no update needed. */
	if (!section_needs_update(&cfg->image_current, &cfg->image, FMAP_SI_ME)) {
		STATUS("[x] - CSME RO version differs to that of the archive.\n");
		return -1;
	}
	STATUS("[✓] - CSME RO has identical version to that in the archive.\n");

	/* cmp csme in archive with running version, if identical no update needed. */
	STATUS("[✓] - [unimpl] CSME running has identical version to that in the archive.\n");

	return 0;
}

static int do_csme(int argc, char *argv[])
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

	if (check_csme_update_possible(cfg) < 0) {
		INFO("NO CSME updates for you!\n");
	} else {
		INFO("CSME firmware update required.\n");
	}

cleanups:
	updater_delete_config(cfg);
	return !!errorcnt;
}
#define CMD_HELP_STR "Update CSME firmware"

#else /* USE_FLASHROM */

static int do_csme(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `csme` subcommand "
		     "unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Update CSME firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(csme, do_csme, VBOOT_VERSION_ALL, CMD_HELP_STR);
