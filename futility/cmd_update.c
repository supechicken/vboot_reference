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
	char *emulation;
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

struct system_property {
	int (*getter)();
	int value;
	int initialized;
};

enum system_property_type {
	SYS_PROP_MAINFW_ACT,
	SYS_PROP_MAX
};

struct updater_config {
	struct firmware_image image, image_current;
	struct firmware_image ec_image, pd_image;
	int try_update;
	int write_protection;
	int emulation;
	struct system_property system[SYS_PROP_MAX];
};

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

int get_system_property(enum system_property_type property_type,
			struct updater_config *cfg)
{
	struct system_property *prop;

	assert(property_type < SYS_PROP_MAX);
	prop = &cfg->system[property_type];
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

static int emulate_system_image(const char *file_name,
				struct firmware_image *image)
{
	if (load_image(file_name, image))
		return -1;

	if (asprintf(&image->emulation,
		     "dummy:emulate=VARIABLE_SIZE,image=%s,size=%u",
		     file_name, image->size) < 0) {
		Error("%s: Failed to allocate buffer for programmer: %s.\n",
		      __FUNCTION__, file_name);
		return -1;
	}
	return 0;
}


static int load_system_image(struct updater_config *cfg,
			     struct firmware_image *image)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.read";

	RETURN_ON_FAILURE(host_flashrom(
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
	free(image->emulation);
	memset(image, 0, sizeof(*image));
}

static const char *decide_rw_target(struct updater_config *cfg,
				    enum target_type target)
{
	const char *a = FMAP_RW_SECTION_A, *b = FMAP_RW_SECTION_B;
	int slot = get_system_property(SYS_PROP_MAINFW_ACT, cfg);

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

	if (cfg->emulation) {
		printf("(emulation) Setting try_next to %s, try_count to %d.\n",
		       slot, tries);
		return 0;
	}

	RETURN_ON_FAILURE(VbSetSystemPropertyString("fw_try_next", slot));
	RETURN_ON_FAILURE(VbSetSystemPropertyInt("fw_try_count", tries));
	return 0;
}

static int write_firmware(struct updater_config *cfg,
			  const struct firmware_image *image,
			  const char *section_name)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.write";
	const char *programmer = cfg->emulation ? image->emulation :
			image->programmer;

	if (vb2_write_file(tmp_file, image->data, image->size) != VB2_SUCCESS) {
		Error("%s: Cannot write temporary file for output: %s\n",
		      __FUNCTION__, tmp_file);
		return -1;
	}
	if (cfg->emulation) {
		printf("%s: (emulation) %s %s from %s to %s.\n",
		       __FUNCTION__,
		       image->emulation ? "Writing" : "Skipped writing",
		       section_name ? section_name : "whole image",
		       image->file_name, programmer);

		if (!image->emulation)
			return 0;
	}
	return host_flashrom(FLASHROM_WRITE, tmp_file, programmer, 1,
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

static struct vb2_gbb_header *find_gbb(const struct firmware_image *image)
{
	struct firmware_section section;
	struct vb2_gbb_header *gbb_header;

	find_firmware_section(&section, image, FMAP_RO_GBB);
	gbb_header = (struct vb2_gbb_header *)section.data;
	/*
	 * futil_valid_gbb_header needs v1 header (GoogleBinaryBlockHeader)
	 * but that should be compatible with vb2_gbb_header
	 */
	if (!futil_valid_gbb_header((GoogleBinaryBlockHeader *)gbb_header,
				    section.size, NULL)) {
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
	struct vb2_gbb_header *gbb_from, *gbb_to;

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
	struct firmware_image *from = &cfg->image_current, *to = &cfg->image;
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

static int is_write_protection_enabled(struct updater_config *cfg)
{
	/* TODO(hungte) Auto detect WP if needed. */
	return cfg->write_protection;
}

enum updater_error_codes {
	UPDATE_ERR_DONE,
	UPDATE_ERR_NEED_RO_UPDATE,
	UPDATE_ERR_NO_IMAGE,
	UPDATE_ERR_SYSTEM_IMAGE,
	UPDATE_ERR_SET_COOKIES,
	UPDATE_ERR_WRITE_FIRMWARE,
	UPDATE_ERR_TARGET,
	UPDATE_ERR_UNKNOWN,
};

static CONST_STRING updater_error_messages[] = {
	[UPDATE_ERR_DONE] = "Done (no error)",
	[UPDATE_ERR_NEED_RO_UPDATE] = "RO changed and no WP. Need full update.",
	[UPDATE_ERR_NO_IMAGE] = "No image to update; try specify with -i.",
	[UPDATE_ERR_SYSTEM_IMAGE] = "Cannot load system active firmware.",
	[UPDATE_ERR_SET_COOKIES] = "Failed writing system flags to try update.",
	[UPDATE_ERR_WRITE_FIRMWARE] = "Failed writing firmware.",
	[UPDATE_ERR_TARGET] = "No valid RW target to update. Abort.",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_try_rw_firmware(struct updater_config *cfg,
				  struct firmware_image *image_from,
				  struct firmware_image *image_to,
				  int wp_enabled)
{
	const char *target;

	preserve_gbb(image_from, image_to);
	if (!wp_enabled && !images_have_same_section(
			image_from, image_to, FMAP_RO_SECTION))
		return UPDATE_ERR_NEED_RO_UPDATE;

	/* TODO(hungte): Support vboot1. */
	target = decide_rw_target(cfg, TARGET_SELF);
	if (target == NULL)
		return UPDATE_ERR_TARGET;

	printf("Checking %s contents...\n", target);
	if (images_have_same_section(image_from, image_to, target)) {
		printf(">> No need to update.\n");
		return UPDATE_ERR_DONE;
	}
	target = decide_rw_target(cfg, TARGET_UPDATE);
	printf(">> Updating %s with trial boots.\n", target);
	if (write_firmware(cfg, image_to, target))
		return UPDATE_ERR_WRITE_FIRMWARE;
	if (set_try_cookies(cfg, target))
		return UPDATE_ERR_SET_COOKIES;
	return UPDATE_ERR_DONE;
}

static int update_rw_firmrware(struct updater_config *cfg,
			       struct firmware_image *image_from,
			       struct firmware_image *image_to)
{
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

	return UPDATE_ERR_DONE;
}

static int update_whole_firmware(struct updater_config *cfg,
				 struct firmware_image *image_to)
{
	printf(">> Updating entire firmware images.\n");
	preserve_images(cfg);

	/* FMAP may be different so we should just update all. */
	if (write_firmware(cfg, image_to, NULL) ||
	    write_optional_firmware(cfg, &cfg->ec_image, NULL) ||
	    write_optional_firmware(cfg, &cfg->pd_image, NULL))
		return UPDATE_ERR_WRITE_FIRMWARE;

	return UPDATE_ERR_DONE;
}

static int update_firmware(struct updater_config *cfg)
{
	int wp_enabled;
	struct firmware_image *image_from = &cfg->image_current,
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

	wp_enabled = is_write_protection_enabled(cfg);

	if (cfg->try_update) {
		int r = update_try_rw_firmware(cfg, image_from, image_to,
					       wp_enabled);
		if (r != UPDATE_ERR_NEED_RO_UPDATE)
			return r;
		printf("Warning: %s\n", updater_error_messages[r]);
	}

	if (wp_enabled)
		return update_rw_firmrware(cfg, image_from, image_to);
	else
		return update_whole_firmware(cfg, image_to);
}

static void unload_updater_config(struct updater_config *cfg)
{
	int i;
	for (i = 0; i < SYS_PROP_MAX; i++) {
		cfg->system[i].initialized = 0;
		cfg->system[i].value = 0;
	}
	free_image(&cfg->image);
	free_image(&cfg->image_current);
	free_image(&cfg->ec_image);
	free_image(&cfg->pd_image);
	cfg->emulation = 0;
}

/* Command line options */
static struct option const long_opts[] = {
	/* name  has_arg *flag val */
	{"image", 1, NULL, 'i'},
	{"ec_image", 1, NULL, 'e'},
	{"pd_image", 1, NULL, 'P'},
	{"try", 0, NULL, 't'},
	{"wp", 1, NULL, 'W'},
	{"emulation", 1, NULL, 'E'},
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
		"    --emulation=FILE\tEmulate system firmware using file.\n"
		"",
		argv[0]);
}

static int do_update(int argc, char *argv[])
{
	int i, errorcnt = 0;
	struct updater_config cfg = {
		.image = { .programmer = PROG_HOST, },
		.image_current = { .programmer = PROG_HOST, },
		.ec_image = { .programmer = PROG_EC, },
		.pd_image = { .programmer = PROG_PD, },
		.try_update = 0,
		.write_protection = 1,
		.emulation = 0,
		.system = {
			[SYS_PROP_MAINFW_ACT] = {.getter = host_get_mainfw_act},
		},
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
		case 'E':
			cfg.emulation = 1;
			errorcnt += emulate_system_image(
					optarg, &cfg.image_current);
			/* Both image and image_current need emulation. */
			if (!errorcnt) {
				cfg.image.emulation = strdup(
						cfg.image_current.emulation);
			}
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
		if (r != UPDATE_ERR_DONE) {
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
