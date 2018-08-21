/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * A reference implementation for AP (and supporting images) firmware updater.
 */

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "crossystem.h"
#include "fmap.h"
#include "futility.h"
#include "host_misc.h"
#include "utility.h"

typedef const char * const CONST_STRING;

#define RETURN_ON_FAILURE(x) do {int r = (x); if (r) return r;} while (0);

/* FMAP section names. */
static CONST_STRING FMAP_RO_SECTION = "RO_SECTION",
		    FMAP_RO_FRID = "RO_FRID",
		    FMAP_RO_GBB = "GBB",
		    FMAP_RO_VPD = "RO_VPD",
		    FMAP_RW_VPD = "RW_VPD",
		    FMAP_RW_SECTION_A = "RW_SECTION_A",
		    FMAP_RW_SECTION_B = "RW_SECTION_B",
		    FMAP_RW_FWID = "RW_FWID",
		    FMAP_RW_FWID_A = "RW_FWID_A",
		    FMAP_RW_FWID_B = "RW_FWID_B",
		    FMAP_RW_SHARED = "RW_SHARED",
		    FMAP_RW_LEGACY = "RW_LEGACY",
		    FMAP_RW_NVRAM = "RW_NVRAM";

/* System environment values. */
static CONST_STRING FWACT_A = "A",
		    FWACT_B = "B";

/* flashrom programmers. */
static CONST_STRING PROG_HOST = "host",
		    PROG_EC = "ec",
		    PROG_PD = "ec:dev=1";

enum target_type {
	TARGET_SELF,
	TARGET_UPDATE,
};

enum active_slot {
	SLOT_UNKNOWN = -1,
	SLOT_A = 0,
	SLOT_B,
};

enum flashrom_ops {
	FLASHROM_READ,
	FLASHROM_WRITE,
};

struct firmware_image {
	const char *programmer;
	uint32_t size;
	uint8_t *data;
	char *file_name;
	char *ro_version, *rw_version_a, *rw_version_b;
	FmapHeader *fmap_header;
};

struct firmware_section {
	uint8_t *data;
	size_t size;
};

struct env_property {
	int (*getter)();
	int value;
	int initialized;
};

enum env_property_types {
	ENV_PROP_MAINFW_ACT,
	ENV_PROP_MAX
};

struct system_env {
	/* Setters or special commands without preset values. */
	int (*flashrom)(enum flashrom_ops op, const char *image_path,
			const char *programmer, int verbose,
			const char *section_name);
	int (*set_property_int)(const char *name, int value);
	int (*set_property_str)(const char *name, const char *value);

	struct env_property properties[ENV_PROP_MAX];
};

struct updater_config {
	struct firmware_image image, old_image;
	struct firmware_image ec_image, pd_image;
	struct system_env env;
	int try_update;
	int write_protection;
	int dryrun;
};

static int host_set_property_int(const char *name, int value)
{
	Debug("%s: VbSetSystemPropertyInt('%s', %d)\n", __FUNCTION__, name,
	      value);
	return VbSetSystemPropertyInt(name, value);
}

static int host_set_property_str(const char *name, const char *value)
{
	Debug("%s: VbSetSystemPropertyString('%s', '%s')\n", __FUNCTION__, name,
	      value);
	return VbSetSystemPropertyString(name, value);
}

static int host_get_mainfw_act()
{
	char buf[VB_MAX_STRING_PROPERTY];

	if (!VbGetSystemPropertyString("mainfw_act", buf, sizeof(buf)))
		return SLOT_UNKNOWN;

	if (strcmp(buf, FWACT_A) == 0)
		return SLOT_A;
	else if (strcmp(buf, FWACT_B) == 0)
		return SLOT_B;

	return SLOT_UNKNOWN;
}

static int host_flashrom(enum flashrom_ops op, const char *image_path,
			 const char *programmer, int verbose,
			 const char *section_name)
{
	char *command;
	const char *op_cmd, *dash_i = "-i", *postfix = "";
	int r;

	if (debugging_enabled)
		verbose = 1;

	if (!verbose)
		postfix = " >/dev/null 2>&1";

	if (!section_name || !*section_name) {
		dash_i = "";
		section_name = "";
	}

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

	/* TODO(hungte) In future we should link with flashrom directly. */
	r = asprintf(&command, "flashrom %s %s -p %s %s %s %s", op_cmd,
		     image_path, programmer, dash_i, section_name, postfix);

	if (r == -1) {
		/* `command` will be not available. */
		Error("%s: Cannot allocate memory for command to execute.\n",
		      __FUNCTION__);
		return -1;
	}

	if (verbose)
		printf("Executing: %s\n", command);

	r = system(command);
	free(command);
	return r;
}

int get_env_property(enum env_property_types env_type, struct system_env *env)
{
	struct env_property *prop;

	assert(env_type < ENV_PROP_MAX);
	prop = &env->properties[env_type];
	if (!prop->initialized) {
		prop->initialized = 1;
		prop->value = prop->getter();
	}
	return prop->value;
}

static int find_firmware_section(struct firmware_section *section,
				 const struct firmware_image *image,
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
	section->data = (uint8_t *)ptr;
	section->size = fah->area_size;
	return 0;
}

static int firmware_section_exists(const struct firmware_image *image,
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
		*version = strndup((const char*)fwid.data, fwid.size);
		return 0;
	}
	*version = strdup("");
	return -1;
}

static int load_image(const char *file_name, struct firmware_image *image)
{
	Debug("%s: Load image file from %s...\n", __FUNCTION__, file_name);

	if (vb2_read_file(file_name, &image->data, &image->size) != VB2_SUCCESS)
	{
		Error("%s: Failed to load %s\n", __FUNCTION__, file_name);
		return -1;
	}

	Debug("%s: Image size: %d\n", __FUNCTION__, image->size);
	assert(image->data);
	image->file_name = strdup(file_name);

	image->fmap_header = fmap_find(image->data, image->size);
	if (!image->fmap_header) {
		Error("Invalid image file (missing FMAP): %s\n", file_name);
		return -1;
	}

	if (!firmware_section_exists(image, FMAP_RO_FRID)) {
		Error("Does not look like VBoot firmware image: %s", file_name);
		return -1;
	}

	load_firmware_version(image, FMAP_RO_FRID, &image->ro_version);
	if (firmware_section_exists(image, FMAP_RW_FWID_A)) {
		char **a = &image->rw_version_a, **b = &image->rw_version_b;
		load_firmware_version(image, FMAP_RW_FWID_A, a);
		load_firmware_version(image, FMAP_RW_FWID_B, b);
	} else if (firmware_section_exists(image, FMAP_RW_FWID)) {
		char **a = &image->rw_version_a, **b = &image->rw_version_b;
		load_firmware_version(image, FMAP_RW_FWID, a);
		load_firmware_version(image, FMAP_RW_FWID, b);
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

static const char *decide_rw_target(struct system_env *env,
				    enum target_type target)
{
	const char *a = FMAP_RW_SECTION_A, *b = FMAP_RW_SECTION_B;
	int slot = get_env_property(ENV_PROP_MAINFW_ACT, env);

	switch (slot) {
	case SLOT_A:
		return target == TARGET_UPDATE ? b : a;

	case SLOT_B:
		return target == TARGET_UPDATE ? a : b;
	}

	return NULL;
}

static int set_try_cookies(struct updater_config *cfg, const char *target)
{
	int tries = 6;
	const char *slot;

	/* EC Software Sync needs few more reboots. */
	if (cfg->ec_image.data)
		tries += 2;

	/* Find new slot according to target (section) name. */
	if (strcmp(target, FMAP_RW_SECTION_A) == 0)
		slot = FWACT_A;
	else if (strcmp(target, FMAP_RW_SECTION_A) == 0)
		slot = FWACT_B;
	else {
		Error("%s: Unknown target: %s\n", __FUNCTION__, target);
		return -1;
	}

	if (cfg->dryrun) {
		printf("(dryrun) Setting try_next to %s, try_count to %d.\n",
		       slot, tries);
		return 0;
	}

	RETURN_ON_FAILURE(cfg->env.set_property_str("fw_try_next", slot));
	RETURN_ON_FAILURE(cfg->env.set_property_int("fw_try_count", tries));
	return 0;
}

static int write_firmware(struct updater_config *cfg,
			  const struct firmware_image *image,
			  const char *section_name)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.write";
	if (vb2_write_file(tmp_file, image->data, image->size) != VB2_SUCCESS) {
		Error("%s: Cannot write temporary file for output: %s\n",
		      __FUNCTION__, tmp_file);
		return -1;
	}
	if (cfg->dryrun) {
		printf("(dryrun) Write %s from %s to using <%s>.\n",
		       section_name ? section_name : "whole image",
		       image->file_name, image->programmer);
		return 0;
	}
	return cfg->env.flashrom(FLASHROM_WRITE, tmp_file, image->programmer, 1,
				 section_name);
}

static int write_optional_firmware(struct updater_config *cfg,
				   const struct firmware_image *image,
				   const char *section_name)
{
	if (!image->data) {
		Debug("%s: No data in <%s> image.\n", __FUNCTION__,
		      image->programmer);
		return 0;
	}
	if (!firmware_section_exists(image, section_name)) {
		Debug("%s: Image %s<%s> does not have section %s.\n",
		      __FUNCTION__, image->file_name, image->programmer,
		      section_name);
		return 0;
	}

	return write_firmware(cfg, image, section_name);
}

static int preserve_firmware_section(const struct firmware_image *image_from,
				     struct firmware_image *image_to,
				     const char *section_name)
{
	struct firmware_section from, to;

	find_firmware_section(&from, image_from, section_name);
	find_firmware_section(&to, image_to, section_name);
	if (!from.data || !to.data)
		return -1;
	if (from.size > to.size) {
		printf("WARNING: %s: Section %s is truncated after updated.\n",
		       __FUNCTION__, section_name);
	}
	/* Use memmove in case if we need to deal with sections that overlap. */
	memmove(to.data, from.data, Min(from.size, to.size));
	return 0;
}

static GoogleBinaryBlockHeader *find_gbb(const struct firmware_image *image)
{
	struct firmware_section section;
	GoogleBinaryBlockHeader *gbb_header;

	find_firmware_section(&section, image, FMAP_RO_GBB);
	gbb_header = (GoogleBinaryBlockHeader *)section.data;
	if (!futil_valid_gbb_header(gbb_header, section.size, NULL)) {
		Error("%s: Cannot find GBB in image: %s.\n", __FUNCTION__,
		      image->file_name);
		return NULL;
	}
	return gbb_header;
}

static int preserve_gbb(const struct firmware_image *image_from,
			struct firmware_image *image_to)
{
	int len;
	uint8_t *hwid_to, *hwid_from;
	GoogleBinaryBlockHeader *gbb_from, *gbb_to;

	gbb_from = find_gbb(image_from);
	gbb_to = find_gbb(image_to);

	if (!gbb_from || !gbb_to)
		return -1;

	/* Preserve flags. */
	gbb_to->flags = gbb_from->flags;
	hwid_to = (uint8_t *)gbb_to + gbb_to->hwid_offset;
	hwid_from = (uint8_t *)gbb_from + gbb_from->hwid_offset;

	/* Preserve HWID. */
	len = strlen((const char *)hwid_from);
	if (len >= gbb_to->hwid_size)
		return -1;

	/* Zero whole area so we won't have garbage after NUL. */
	memset(hwid_to, 0, gbb_to->hwid_size);
	memcpy(hwid_to, hwid_from, len);
	return 0;
}

static int preserve_images(struct updater_config *cfg)
{
	int errcnt = 0;
	struct firmware_image *from = &cfg->old_image, *to = &cfg->image;
	errcnt += preserve_gbb(from, to);
	errcnt += preserve_firmware_section(from, to, FMAP_RO_VPD);
	errcnt += preserve_firmware_section(from, to, FMAP_RW_VPD);
	errcnt += preserve_firmware_section(from, to, FMAP_RW_NVRAM);
	return errcnt;
}

static int compare_section(const struct firmware_section *a,
			   const struct firmware_section *b)
{
	if (a->size != b->size)
		return a->size - b->size;
	return memcmp(a->data, b->data, a->size);
}

static int images_have_same_section(const struct firmware_image *image_from,
				    const struct firmware_image *image_to,
				    const char *section_name)
{
	struct firmware_section from, to;

	find_firmware_section(&from, image_from, section_name);
	find_firmware_section(&to, image_to, section_name);
	return compare_section(&from, &to) == 0;
}
enum updater_error_codes {
	UPDATE_ERR_NONE,
	UPDATE_ERR_NO_IMAGE,
	UPDATE_ERR_SYSTEM_IMAGE,
	UPDATE_ERR_SET_COOKIES,
	UPDATE_ERR_WRITE_FIRMWARE,
	UPDATE_ERR_TARGET,
	UPDATE_ERR_UNKNOWN,
};

static CONST_STRING updater_error_messages[] = {
	[UPDATE_ERR_NONE] = "None",
	[UPDATE_ERR_NO_IMAGE] = "No image to update; try specify with -i.",
	[UPDATE_ERR_SYSTEM_IMAGE] = "Cannot load system active firmware.",
	[UPDATE_ERR_SET_COOKIES] = "Failed writing system flags to try update.",
	[UPDATE_ERR_WRITE_FIRMWARE] = "Failed writing firmware.",
	[UPDATE_ERR_TARGET] = "No valid RW target to update. Abort.",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_firmware(struct updater_config *cfg)
{
	int wp_enabled;
	struct firmware_image *image_from = &cfg->old_image,
			      *image_to = &cfg->image;
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

	/* Use while so we can use 'break' to fallback to RO+RW update mode. */
	while (cfg->try_update) {
		const char *target;

		preserve_gbb(image_from, image_to);
		if (!images_have_same_section(image_from, image_to,
					      FMAP_RO_SECTION) && !wp_enabled) {
			printf("WP disabled and RO changed. Do full update.\n");
			break;
		}
		/* TODO(hungte): Support vboot1. */
		target = decide_rw_target(&cfg->env, TARGET_SELF);
		if (target == NULL) {
			return UPDATE_ERR_TARGET;
		}
		printf("Checking %s contents...\n", target);
		if (images_have_same_section(image_from, image_to, target)) {
			printf(">> No need to update.\n");
			return UPDATE_ERR_NONE;
		}

		target = decide_rw_target(&cfg->env, TARGET_UPDATE);
		printf(">> Updating %s with trial boots.\n", target);
		if (write_firmware(cfg, image_to, target))
			return UPDATE_ERR_WRITE_FIRMWARE;
		if (set_try_cookies(cfg, target))
			return UPDATE_ERR_SET_COOKIES;
		return UPDATE_ERR_NONE;
	}

	if (wp_enabled) {
		printf(">> Updating %s, %s, and %s.\n", FMAP_RW_SECTION_A,
		       FMAP_RW_SECTION_B, FMAP_RW_SHARED);
		/*
		 * TODO(hungte) Speed up by flashing multiple sections in one
		 * command, or provide diff file.
		 */
		if (write_firmware(cfg, image_to, FMAP_RW_SECTION_A) ||
		    write_firmware(cfg, image_to, FMAP_RW_SECTION_B) ||
		    write_firmware(cfg, image_to, FMAP_RW_SHARED))
			return UPDATE_ERR_WRITE_FIRMWARE;

		if (firmware_section_exists(image_to, FMAP_RW_LEGACY) &&
		    write_firmware(cfg, image_to, FMAP_RW_LEGACY))
			return UPDATE_ERR_WRITE_FIRMWARE;
	} else {
		printf(">> Updating entire firmware images.\n");
		preserve_images(cfg);

		/* FMAP may be different so we should just update all. */
		if (write_firmware(cfg, image_to, NULL) ||
		    write_optional_firmware(cfg, &cfg->ec_image, NULL) ||
		    write_optional_firmware(cfg, &cfg->pd_image, NULL))
			return UPDATE_ERR_WRITE_FIRMWARE;
	}
	return UPDATE_ERR_NONE;
}

static void clear_env(struct system_env *env)
{
	int i;
	for (i = 0; i < ENV_PROP_MAX; i++) {
		env->properties[i].initialized = 0;
		env->properties[i].value = 0;
	}
}

static void unload_updater_config(struct updater_config *cfg)
{
	clear_env(&cfg->env);
	free_image(&cfg->image);
	free_image(&cfg->old_image);
	free_image(&cfg->ec_image);
	free_image(&cfg->pd_image);
}

/* Command line options */
static struct option const long_opts[] = {
	/* name  has_arg *flag val */
	{"image", 1, NULL, 'i'},
	{"ec_image", 1, NULL, 'e'},
	{"pd_image", 1, NULL, 'P'},
	{"try", 0, NULL, 't'},
	{"wp", 1, NULL, 'W'},
	{"dryrun", 0, NULL, 'D'},
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static CONST_STRING short_opts = "hi:e:t";

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [OPTIONS]\n"
		"\n"
		"-i, --image=FILE    \tAP (host) firmware image (image.bin)\n"
		"-e, --ec_image=FILE \tEC firmware image (i.e, ec.bin)\n"
		"    --pd_image=FILE \tPD firmware image (i.e, pd.bin)\n"
		"-t, --try           \tUse A/B trial update if possible\n"
		"\n"
		"Debugging and testing options:\n"
		"    --wp=1|0        \tSpecify write protection status\n"
		"    --dryrun        \tDo not make modification to system\n"
		"",
		argv[0]);
}

static int do_update(int argc, char *argv[])
{
	int i, errorcnt = 0;
	struct updater_config cfg = {
		.image = { .programmer = PROG_HOST, },
		.old_image = { .programmer = PROG_HOST, },
		.ec_image = { .programmer = PROG_EC, },
		.pd_image = { .programmer = PROG_PD, },
		.env = {
			.flashrom = host_flashrom,
			.set_property_int = host_set_property_int,
			.set_property_str = host_set_property_str,
			.properties = {
				[ENV_PROP_MAINFW_ACT] = {
					.getter = host_get_mainfw_act,
				},
			},
		},
		.try_update = 0,
		.write_protection = 1,
	};

	printf(">> Firmware updater started.\n");

	opterr = 0;
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'i':
			errorcnt += load_image(optarg, &cfg.image);
			break;
		case 'e':
			errorcnt += load_image(optarg, &cfg.ec_image);
			break;
		case 'P':
			errorcnt += load_image(optarg, &cfg.pd_image);
			break;
		case 't':
			cfg.try_update = 1;
			break;
		case 'W':
			cfg.write_protection = atoi(optarg);
			break;
		case 'D':
			cfg.dryrun = 1;
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
	if (optind < argc) {
		errorcnt++;
		Error("Unexpected arguments.\n");
	}
	if (!errorcnt) {
		int r = update_firmware(&cfg);
		if (r != UPDATE_ERR_NONE) {
			r = Min(r, UPDATE_ERR_UNKNOWN);
			Error("%s\n", updater_error_messages[r]);
			errorcnt++;
		}
	}
	printf(">> %s: Firmware updater %s.\n",
	       errorcnt ? "FAILED": "DONE",
	       errorcnt ? "stopped due to error" : "exited successfully");

	unload_updater_config(&cfg);
	return !!errorcnt;
}

DECLARE_FUTIL_COMMAND(update, do_update, VBOOT_VERSION_ALL,
		      "Update system firmware");
