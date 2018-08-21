/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * A reference implementation for AP (and supporting images) firmware updater.
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "fmap.h"
#include "futility.h"

#define STRING_LIST_ALLOCATION_UNIT 10
#define COMMAND_BUFFER_SIZE 512
#define RETURN_ON_FAILURE(x) do {int r = (x); if (r) return r;} while (0);
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* FMAP section names. */
static const char *RO_FRID = "RO_FRID",
		  *RW_A = "RW_SECTION_A",
		  *RW_B = "RW_SECTION_B",
		  *RW_FWID = "RW_FWID",
		  *RW_FWID_A = "RW_FWID_A",
		  *RW_FWID_B = "RW_FWID_B",
		  *RW_SHARED = "RW_SHARED",
		  *RW_LEGACY = "RW_LEGACY";
/* flashrom programmers. */
static const char *PROG_HOST = "host",
		  *PROG_EC = "ec",
		  *PROG_PD = "ec:dev=1";

enum flashrom_ops {
	FLASHROM_READ,
	FLASHROM_WRITE,
	FLASHROM_WP_STATUS,
};

struct firmware_image {
	const char *programmer;
	size_t size;
	uint8_t *data;
	char *file_name;
	char *ro_version, *rw_version_a, *rw_version_b;
	FmapHeader *fmap_header;
};

struct firmware_image_set {
	struct firmware_image image, ec_image, pd_image;
};

struct firmware_section {
	char *data;
	size_t size;
};

struct system_env {
	int (*flashrom)(enum flashrom_ops op, const char *image_path,
			const char *programmer, int verbose,
			const char *section_name);
};

struct updater_config {
	struct firmware_image_set from, to;
	struct system_env env;
	int try_update;
	int write_protection;
};

static int host_flashrom(enum flashrom_ops op, const char *image_path,
			 const char *programmer, int verbose,
			 const char *section)
{
	/* TODO(hungte) Create command buffer dynamically. */
	char buf[COMMAND_BUFFER_SIZE];
	const char *op_cmd;

	switch (op) {
	case FLASHROM_READ:
		op_cmd = "-r";
		assert(image_path);
		break;

	case FLASHROM_WRITE:
		op_cmd = "-w";
		assert(image_path);
		break;

	default:
		assert(0);
		return -1;
	}

	snprintf(buf, sizeof(buf), "flashrom %s %s -p %s %s %s", op_cmd,
		 image_path, programmer, section ? "-i" : "",
		 section ? section : "");

	if (verbose || debugging_enabled)
		printf("Executing: %s\n", buf);

	return system(buf);
}

static int find_firmware_section(struct firmware_section *section,
				 struct firmware_image *image,
				 const char *section_name)
{
	FmapAreaHeader *fah = NULL;
	uint8_t *ptr;

	section->data = NULL;
	section->size = 0;
	ptr = fmap_find_by_name(
			image->data, image->size, image->fmap_header,
			section_name, &fah);
	if (!ptr)
		return -1;
	section->data = (char *)ptr;
	section->size = fah->area_size;
	return 0;
}

static int firmware_section_exists(struct firmware_image *image,
				   const char *section_name)
{
	struct firmware_section section;
	find_firmware_section(&section, image, section_name);
	return section.data != NULL;
}

static int load_firmware_version(struct firmware_image *image,
				  const char *section_name,
				  char **version)
{
	struct firmware_section fwid;
	find_firmware_section(&fwid, image, section_name);
	if (fwid.size) {
		*version = strdup(fwid.data);
		return 0;
	}
	*version = strdup("");
	return -1;
}

static int load_image(const char *file_name, struct firmware_image *image)
{
	int len;
	FILE *fp = fopen(file_name, "rb");

	Debug("%s: Load image file from %s...\n", __FUNCTION__, file_name);
	if (!fp) {
		Error("Failed to load %s\n", file_name);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	image->size = ftell(fp);
	rewind(fp);
	Debug("%s: Image size: %d\n", __FUNCTION__, image->size);

	image->file_name = strdup(file_name);
	image->data = (uint8_t *)malloc(image->size);
	assert(image->data);

	len = fread(image->data, image->size, 1, fp);
	fclose(fp);

	if (len != 1) {
		Error("Fail to read data from: %s\n", file_name);
		return -1;
	}

	image->fmap_header = fmap_find(image->data, image->size);
	if (!image->fmap_header) {
		Error("Invalid image file (missing FMAP): %s\n", file_name);
		return -1;
	}

	if (!firmware_section_exists(image, RO_FRID)) {
		Error("Does not look like VBoot firmware image: %s", file_name);
		return -1;
	}

	load_firmware_version(image, RO_FRID, &image->ro_version);
	if (firmware_section_exists(image, RW_FWID_A)) {
		load_firmware_version(image, RW_FWID_A, &image->rw_version_a);
		load_firmware_version(image, RW_FWID_B, &image->rw_version_b);
	} else if (firmware_section_exists(image, RW_FWID)) {
		load_firmware_version(image, RW_FWID, &image->rw_version_a);
		load_firmware_version(image, RW_FWID, &image->rw_version_b);
	} else {
		Error("Unsupported VBoot firmware (no RW ID): %s", file_name);
	}
	return 0;
}

static int load_system_image(struct updater_config *cfg,
			     struct firmware_image *image)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.read";

	RETURN_ON_FAILURE(cfg->env.flashrom(
			FLASHROM_READ, tmp_file, image->programmer, 0, NULL));
	return load_image(tmp_file, image);
}

static void free_image(struct firmware_image *image)
{
	free(image->data);
	free(image->file_name);
	free(image->ro_version);
	free(image->rw_version_a);
	free(image->rw_version_b);
	memset(image, 0, sizeof(*image));
}

static int write_firmware(struct updater_config *cfg,
			  struct firmware_image *image,
			  const char *section)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.write";
	FILE *fp = fopen(tmp_file, "wb");
	if (!fp)
		return -1;
	fwrite(image->data, image->size, 1, fp);
	fclose(fp);
	return cfg->env.flashrom(FLASHROM_WRITE, tmp_file, image->programmer, 1,
				 section);
}

static int write_optional_firmware(struct updater_config *cfg,
				   struct firmware_image *image,
				   const char *section)
{
	if (!image->data)
		return 0;
	return write_firmware(cfg, image, section);
}

enum updater_error_codes {
	UPDATE_ERR_NONE,
	UPDATE_ERR_NO_IMAGE,
	UPDATE_ERR_SYSTEM_IMAGE,
	UPDATE_ERR_UNKNOWN,
};

static const char *updater_error_messages[] = {
	[UPDATE_ERR_NONE] = "None",
	[UPDATE_ERR_NO_IMAGE] = "No image to update; try specify with -i.",
	[UPDATE_ERR_SYSTEM_IMAGE] = "Cannot load system active firmware.",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_firmware(struct updater_config *cfg)
{
	int wp_enabled;
	struct firmware_image *image_from = &cfg->from.image,
			      *image_to = &cfg->to.image;
	if (!image_to->data)
		return UPDATE_ERR_NO_IMAGE;

	printf(">> Target image: %s (RO:%s, RW/A:%s, RW/B:%s).\n",
	       image_to->file_name, image_to->ro_version,
	       image_to->rw_version_a, image_to->rw_version_b);

	if (!image_from->data) {
		/*
		 * TODO(hungte) Read only RO_SECTION, VBLOCK_A, VBLOCK_B,
		 * RO_VPD, RW_VPD, RW_NVRAM, RW_LEGACY.
		 */
		printf("Loading current system firmware...\n");
		if (load_system_image(cfg, image_from) != 0)
			return UPDATE_ERR_SYSTEM_IMAGE;
	}
	printf(">> Current system: %s (RO:%s, RW/A:%s, RW/B:%s).\n",
	       image_from->file_name, image_from->ro_version,
	       image_from->rw_version_a, image_from->rw_version_b);

	/* TODO(hungte) Auto detect WP if needed. */
	wp_enabled = cfg->write_protection;

	if (cfg->try_update) {
		Error("Not supported yet.\n");
		return UPDATE_ERR_UNKNOWN;
	}

	if (wp_enabled) {
		printf(">> Updating %s, %s, and %s.\n", RW_A, RW_B, RW_SHARED);

		write_firmware(cfg, image_to, RW_A);
		write_firmware(cfg, image_to, RW_B);
		write_firmware(cfg, image_to, RW_SHARED);

		if (firmware_section_exists(image_to, RW_LEGACY))
			write_firmware(cfg, image_to, RW_LEGACY);
	} else {
		printf(">> Updating entire firmware images.\n");

		/* FMAP may be different so we should just update all. */
		write_firmware(cfg, image_to, NULL);
		write_optional_firmware(cfg, &cfg->to.ec_image, NULL);
		write_optional_firmware(cfg, &cfg->to.pd_image, NULL);
	}
	return UPDATE_ERR_NONE;
}

static void unload_updater_config(struct updater_config *cfg)
{
	free_image(&cfg->to.image);
	free_image(&cfg->to.ec_image);
	free_image(&cfg->to.pd_image);
	free_image(&cfg->from.image);
	free_image(&cfg->from.ec_image);
	free_image(&cfg->from.pd_image);
}
/* Command line options */
static struct option long_opts[] = {
	/* name  has_arg *flag val */
	{"image", 1, NULL, 'i'},
	{"ec_image", 1, NULL, 'e'},
	{"pd_image", 1, NULL, 'P'},
	{"try", 0, NULL, 't'},
	{"wp", 1, NULL, 'W'},
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static const char *short_opts = "i:e:t";
static int errorcnt = 0;

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [OPTIONS]\n"
		"\n"
		"-i, --image=FILE   \tAP (host) firmware image (image.bin)\n"
		"-e, --ec_image=FILE\tEC firmware image (i.e, ec.bin)\n"
		"    --pd_image=FILE\tPD firmware image (i.e, pd.bin)\n"
		"-t, --try          \tUse A/B trial update if possible\n"
		"    --wp=1|0       \tSpecify write protection status\n"
		"",
		argv[0]);
}

static int do_update(int argc, char *argv[])
{
	int i;
	struct updater_config cfg = {
		.from = {
			.image = { .programmer = PROG_HOST, },
			.ec_image = { .programmer = PROG_EC, },
			.pd_image = { .programmer = PROG_PD, },
		},
		.to = {
			.image = { .programmer = PROG_HOST, },
			.ec_image = { .programmer = PROG_EC, },
			.pd_image = { .programmer = PROG_PD, },
		},
		.env = {
			.flashrom = host_flashrom,
		},
		.try_update = 0,
		.write_protection = 1,
	};

	opterr = 0;		/* quiet, you */
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'i':
			errorcnt += load_image(optarg, &cfg.to.image);
			break;
		case 'e':
			errorcnt += load_image(optarg, &cfg.to.ec_image);
			break;
		case 'P':
			errorcnt += load_image(optarg, &cfg.to.pd_image);
			break;
		case 't':
			cfg.try_update = 1;
			break;
		case 'W':
			cfg.write_protection = atoi(optarg);
			break;
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

	unload_updater_config(&cfg);
	return !!errorcnt;
}

DECLARE_FUTIL_COMMAND(update, do_update, VBOOT_VERSION_ALL,
		      "Update system firmware");
