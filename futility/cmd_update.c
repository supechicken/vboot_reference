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

#include "futility.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* flashrom programmers. */
static const char *PROG_HOST = "host",
		  *PROG_EC = "ec",
		  *PROG_PD = "ec:dev=1";

struct firmware_image {
	const char *programmer;
	size_t size;
	uint8_t *data;
	char *file_name;
	char *ro_version, *rw_version_a, *rw_version_b;
};

struct firmware_image_set {
	struct firmware_image image, ec_image, pd_image;
};

struct updater_config {
	struct firmware_image_set from, to;
};

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

	return 0;
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

enum updater_error_codes {
	UPDATE_ERR_NONE,
	UPDATE_ERR_UNKNOWN,
};

static const char *updater_error_messages[] = {
	[UPDATE_ERR_NONE] = "None",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_firmware(struct updater_config *cfg)
{
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
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static const char *short_opts = "i:e:";
static int errorcnt = 0;

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [OPTIONS]\n"
		"\n"
		"-i, --image=FILE   \tAP (host) firmware image (image.bin)\n"
		"-e, --ec_image=FILE\tEC firmware image (i.e, ec.bin)\n"
		"    --pd_image=FILE\tPD firmware image (i.e, pd.bin)\n"
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
