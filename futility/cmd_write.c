/* Copyright 2023 The ChromiumOS Authors
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
#include <fcntl.h>
#include <sys/mman.h>

#include "flash_helpers.h"
#include "futility.h"
#include "updater.h"

#ifdef USE_FLASHROM

/* Write firmware to flash. Takes ownership of inbuf and outbuf data. */
static int write_to_flash(struct updater_config *cfg,
			  uint8_t *data, uint32_t data_len,
			  const char *const regions[], size_t r_len)
{
	if (is_write_protection_enabled(cfg)) {
		ERROR("You must disable write protection before setting flags.\n");
		return -1;
	}
	cfg->image.data = data;
	cfg->image.size = data_len;

	int ret = write_system_firmware(cfg, &cfg->image, regions, r_len);

	cfg->image.data = NULL;
	cfg->image.size = 0;
	return ret;
}

static int map_write_file(const char *path,
	void **base_of_rom, size_t *size_of_rom)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		ERROR("%s: can't open %s: %s\n",
			__func__, path, strerror(errno));
		return -1;
	}

	struct stat sb;
	if (fstat(fd, &sb)) {
		ERROR("%s: can't stat %s: %s\n",
			__func__, path, strerror(errno));
		close(fd);
		return -1;
	}

	*base_of_rom = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (*base_of_rom == MAP_FAILED) {
		ERROR("%s: can't mmap %s: %s\n",
			__func__, path, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	*size_of_rom = sb.st_size;

	return 0;
}

static int write_file_to_region(struct updater_config *cfg,
	const char *path, const char *regions)
{
	/* full image write. */
	if (!regions) {
		ERROR("Whole unnconditional image write is currently unimplemented.\n");
		return -1; /* unimplemented */
	}

	/* Map input file into memory. */
	void *base_of_rom = NULL;
	size_t size_of_rom = 0;
	if (map_write_file(path, &base_of_rom, &size_of_rom) < 0) {
		ERROR("Could not map file to write to flash\n");
		return -1;
	}

	int ret = 0;
	char *savedptr;
	char *region = strtok_r((char *)regions, ",", &savedptr);
	while (region) {
		/* Determine the region offset using the FMAP embedded with the file. */
		FmapAreaHeader *fmap_region = NULL;
		uint8_t *r = fmap_find_by_name(base_of_rom, size_of_rom,
					       NULL, region, &fmap_region);
		if (!r || !fmap_region) {
			ERROR("Could not find '%s' in the FMAP\n", region);
			return -1;
		}

		uint8_t *r_data = base_of_rom + fmap_region->area_offset;
		off_t r_len = fmap_region->area_size;
		const char *const r_sel[] = { region };
		if (write_to_flash(cfg, r_data, r_len, r_sel, ARRAY_SIZE(r_sel)) < 0) {
			ret = -1;
			break;
		}

		/* next region to read.. */
		region = strtok_r(NULL, ",", &savedptr);
	}

	if (munmap(base_of_rom, size_of_rom)) {
		ERROR("%s: can't munmap %s: %s\n",
			__func__, path, strerror(errno));
		return -1;
	}
	return ret;
}

/* Command line options */
static struct option const long_opts[] = {
	SHARED_FLASH_ARGS_LONGOPTS
	/* name  has_arg *flag val */
	{"help", 0, NULL, 'h'},
	{"debug", 0, NULL, 'd'},
	{"region", 1, NULL, 'r'},
	{"verbose", 0, NULL, 'v'},
	{NULL, 0, NULL, 0},
};

static const char *const short_opts = "hdrv" SHARED_FLASH_ARGS_SHORTOPTS;

static void print_help(int argc, char *argv[])
{
	printf("\n"
	       "Usage:  " MYNAME " %s [OPTIONS] FILE\n"
	       "\n"
	       "Writes AP firmware from a FILE\n"
	       "-d, --debug         \tPrint debugging messages\n"
	       "-r, --region        \tThe comma delimited regions to read (optional)\n"
	       "-v, --verbose       \tPrint verbose messages\n"
	       SHARED_FLASH_ARGS_HELP,
	       argv[0]);
}

static int do_write(int argc, char *argv[])
{
	struct updater_config *cfg = NULL;
	struct updater_config_arguments args = {0};
	int i;
	char *regions = NULL;

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
		case 'r':
			regions = optarg;
			break;
		case 'v':
			args.verbosity++;
			break;
		case '?':
			if (optopt)
				ERROR("Unrecognized option: -%c\n", optopt);
			else if (argv[optind - 1])
				ERROR("Unrecognized option (possibly '%s')\n",
				      argv[optind - 1]);
			else
				ERROR("Unrecognized option.\n");
			return 1;
		default:
			ERROR("Failed parsing options.\n");
			return 1;
		}
	}
	if (argc - optind < 1) {
		ERROR("\nERROR: missing output filename\n");
		print_help(argc, argv);
		return 1;
	}

	if (setup_flash(&cfg, &args)) {
		ERROR("While preparing flash\n");
		return 1;
	}

	int errorcnt = 0;
	int r = load_system_firmware(cfg, &cfg->image_current);
	/*
	 * Ignore a parse error as we still want to write.
	 */
	if (r && r != IMAGE_PARSE_FAILURE) {
		WARN("Image on SPI flash has parse error corruptions\n");
		errorcnt++;
		goto err;
	}

	const char *path = argv[optind++];
	if (write_file_to_region(cfg, path, regions) < 0)
		errorcnt++;

err:
	teardown_flash(cfg);
	return !!errorcnt;
}
#define CMD_HELP_STR "Write AP firmware"

#else /* USE_FLASHROM */

static int do_write(int argc, char *argv[])
{
	FATAL(MYNAME " was built without flashrom support, `write` command unavailable!\n");
	return -1;
}
#define CMD_HELP_STR "Write system firmware (unavailable in this build)"

#endif /* !USE_FLASHROM */

DECLARE_FUTIL_COMMAND(write, do_write, VBOOT_VERSION_ALL, CMD_HELP_STR);
