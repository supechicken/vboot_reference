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
		fprintf(stderr, "Unable to open file %s\n", filename);
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
		fprintf(stderr, "Unable to read from file %s\n", filename);
		goto err;
	}

	fclose(f);
	return buf;

err:
	fclose(f);
	free(buf);
	return NULL;
}

// Extract the ME RO Header offset from the ME in firmware updater archive.
static uint32_t extract_me_ro_hdr_offset(struct firmware_section *si_desc)
{
	const uint8_t me_layout_off = 16;
	const uint8_t me_ro_off = me_layout_off + 8;

	// Read current ME's RO Partition offset.
	// ME RO header (BPDT1) starts at the same offset as ME RO Partition offset.
	const uint32_t me_ro_hdr_off = si_desc->data[me_ro_off]      | (si_desc->data[me_ro_off+1] << 8)
				| (si_desc->data[me_ro_off+2] << 16) | (si_desc->data[me_ro_off+3] << 24);

	if (!me_ro_hdr_off)
		fprintf(stderr, "ME RO Header offset can't be read from the archive. Try in the next boot.\n");
	return me_ro_hdr_off;
}

// Extract the ME RO Header block from the current ME in SPI ROM.
static uint8_t *extract_cur_me_ro_hdr_block(struct firmware_section *si_desc, unsigned int *me_ro_hdr_blk_sz)
{
	const int me_region_off = 4096;
	// Ensure that the block starts at 4K boundary.
	const uint32_t me_ro_hdr_blk_start = (me_region_off + extract_me_ro_hdr_offset(si_desc)) & 0xFFFFF000;
	// # Read 2 blocks just in case RO HDR happens to be at the block boundary.
	const uint32_t me_ro_hdr_blk_end = me_ro_hdr_blk_start + 8192 - 1;

	*me_ro_hdr_blk_sz = me_ro_hdr_blk_end - me_ro_hdr_blk_start;
	uint8_t *me_ro_hdr_blk = calloc(1, *me_ro_hdr_blk_sz);
	if (!me_ro_hdr_blk)
		return NULL;
	memcpy(me_ro_hdr_blk, &si_desc->data[me_ro_hdr_blk_start], *me_ro_hdr_blk_sz);

	return me_ro_hdr_blk;
}

static bool is_csme_locked(struct updater_config *cfg)
{
	if (load_system_firmware(cfg, &cfg->image_current)) {
		fprintf(stderr, "could not load firmware image.\n");
		return true;
	}

	// Extract the SI_DESC region
	struct firmware_section si_desc;
	if (find_firmware_section(&si_desc, &cfg->image_current, FMAP_SI_DESC)) {
		fprintf(stderr, "Could not find SI_DESC region. Try in the next boot.\n");
		return true;
	}
	if (si_desc.size == 0) {
		fprintf(stderr, "SI_DESC region is zero sized.\n");
		return true;
	}

	// Check if the value at offset 0x10 is FLVALSIG 0x0ff0a55a.
	const uint32_t flvalsig = si_desc.data[0x10] | (si_desc.data[0x11] << 8)
			| (si_desc.data[0x12] << 16) | (si_desc.data[0x13] << 24);
	if (flvalsig != 0x0ff0a55a) {
		fprintf(stderr, "Valid Flash Descriptor signature not found. ME is locked. \n");
		return true;
	}

	// Extract the flash master base address at 0x18 and use flash master base address as
	// an offset to extract flash master record. Check if bit 22 is set in flash master record.
	const uint8_t fmba = si_desc.data[0x18] << 4;
	const uint32_t flmstr = si_desc.data[fmba]     | (si_desc.data[fmba+1] << 8)
			| (si_desc.data[fmba+2] << 16) | (si_desc.data[fmba+3] << 24);
	if (( flmstr & (1 << 22) ) == 0) {
		fprintf(stderr, "Host CPU does not have write access to ME. ME is locked.\n");
		return true;
	}

	// We cannot extract RO HDR block from current ME. Treat ME as locked.
	unsigned int me_ro_hdr_blk_sz;
	uint8_t *me_ro_hdr_blk;
	if ((me_ro_hdr_blk = extract_cur_me_ro_hdr_block(&si_desc, &me_ro_hdr_blk_sz)) == NULL) {
		fprintf(stderr, "Current ME RO header can't be read. Try in the next boot.\n");
		return true;
	}
	// Check if all bytes in CUR_ME_RO_HDR_BLK are 0xFF. If so ME is locked.
	if (!memcmp(me_ro_hdr_blk, 0xFF, me_ro_hdr_blk_sz))
		return true;

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
		fprintf(stderr, "%s: match failed: %s\n", __func__, msgbuf);
	}

err:
	free(buf);
	return finding;
}

static int check_csme_update_possible(struct updater_config *cfg)
{

	// Returns true if wp is enabled on current system.
	if (is_write_protection_enabled(cfg))
		return -1; /* cannot update csme. */
	STATUS("[✓] - Write Protection is off.\n");

	/* is csme locked? if so no update needed. */
	if (is_csme_locked(cfg))
		return -1;
	STATUS("[✓] - CSME is unlocked.\n");

	/* is csme mp mode? if so no update needed. */
	int ret = is_csme_mp_mode();
	if (ret == 1) { /* match */
		STATUS("[x] - CSME is not in MP mode.\n");
		return -1;
	} else if (ret <= 0) /* no match or error. */
		return ret;
	STATUS("[✓] - CSME is in MP mode.\n");

	/* cmp csme ro versions, if identical no update needed. */
	if (!section_needs_update(&cfg->image_current, &cfg->image, FMAP_SI_ME))
		return -1;
	STATUS("[✓] - CSME RO has identical version to that in the archive.\n");

	/* cmp csme in archive with running version, if identical no update
	 * needed. */
	STATUS("[✓] - CSME running has identical version to that in the archive.\n");

	return 0;
}

/* Generate boot alert regarding an important update. */
static void generate_boot_alert(void)
{
	(void)system("chromeos-boot-alert update_csme_firmware");
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
	// flashrom -p host -i SI_ME -w "${ARCHIVE_BIOS}" >/dev/null
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
			fprintf(stderr, "ERROR: error while parsing options\n");
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
	FATAL(MYNAME " was built without flashrom support, `csme` subcommand "
		     "unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Update CSME firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(csme, do_csme, VBOOT_VERSION_ALL, CMD_HELP_STR);
