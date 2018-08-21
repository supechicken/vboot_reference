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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "fmap.h"
#include "futility.h"
#include "vboot_api.h"

#define STRING_LIST_ALLOCATION_UNIT 10
#define COMMAND_BUFFER_SIZE 256
#define RETURN_ON_FAILURE(x) do {int r = (x); if (r) return r;} while (0);
#define MIN(x, y) ((x) < (y) ? (x) : (y))

struct string_list {
	int capacity;
	int size;
	char **data;
};

/* FMAP section names. */
static const char *RO_ALL = "RO_SECTION",
		  *RO_FRID = "RO_FRID",
		  *RO_GBB = "GBB",
		  *RO_VPD = "RO_VPD",
		  *RW_VPD = "RW_VPD",
		  *RW_A = "RW_SECTION_A",
		  *RW_B = "RW_SECTION_B",
		  *RW_FWID = "RW_FWID",
		  *RW_FWID_A = "RW_FWID_A",
		  *RW_FWID_B = "RW_FWID_B",
		  *RW_SHARED = "RW_SHARED",
		  *RW_LEGACY = "RW_LEGACY",
		  *RW_NVRAM = "RW_NVRAM";

/* System environment values. */
static const char *FWACT_A = "A",
	          *FWACT_B = "B",
		  *WPSW_DISABLED = "0",
		  *FLASHROM_WP_ENABLED = "write protect is enabled",
		  *FLASHROM_WP_DISABLED = "write protect is disabled";

/* flashrom programmers. */
static const char *PROG_HOST = "host",
		  *PROG_EC = "ec",
		  *PROG_PD = "ec:dev=1";

enum target_type {
	TARGET_SELF,
	TARGET_UPDATE
};

enum wp_state {
	WP_AUTODETECT = -1,
	WP_DISABLED,
	WP_ENABLED,
};

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
	/* Getters. */
	const char *(*get_mainfw_act)(struct system_env *env);
	const char *(*get_tpm_fwver)(struct system_env *env);
	const char *(*get_wpsw)(struct system_env *env);

	/* Setter or special commands. */
	int (*flashrom)(enum flashrom_ops op, const char *image_path,
			const char *programmer, int verbose,
			const struct string_list *sections);
	int (*crossystem)(const char *property, const char *value);

	/* Cached or preset values. */
	char *_mainfw_act;
	char *_tpm_fwver;
	char *_wpsw;
};

struct updater_config {
	struct firmware_image_set from, to;
	struct system_env env;
	int try_update;
	int write_protection;

};

static void strip(char *s)
{
	int len;
	assert(s);

	len = strlen(s);
	while (len-- > 0) {
		if (!isascii(s[len]) || !isblank(s[len]))
			break;
		s[len] = '\0';
	}
}

static void free_string_list(struct string_list *list)
{
	int i = 0;
	for (i = 0; i < list->size; i++) {
		free(list->data[i]);
	}
	list->size = 0;
	free(list->data);
	list->data = NULL;
	list->capacity = 0;
}

static void append_string_list(struct string_list *list, const char *element)
{
	if (list->size >= list->capacity) {
		list->capacity += STRING_LIST_ALLOCATION_UNIT;
		assert(list->capacity > list->size);
		list->data = (char **)realloc(
				(void *)list->data,
				list->capacity * sizeof(char *));
		assert(list->data);
	}
	assert(list->data && list->size < list->capacity);
	list->data[list->size++] = strdup(element);
}

static char *host_shell(const char *command)
{
	/* Currently all commands we use do not have large output. */
	char buf[COMMAND_BUFFER_SIZE];

	int result;
	FILE *fp = popen(command, "r");

	Debug("%s: %s\n", __FUNCTION__, command);
	buf[0] = '\0';
	if (!fp) {
		Debug("%s: Execution error for %s.\n", __FUNCTION__, command);
		return strdup(buf);
	}

	fgets(buf, sizeof(buf), fp);
	strip(buf);
	result = pclose(fp);
	if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
		Debug("%s: Execution failure with exit code %d: %s\n",
		      __FUNCTION__, command, WEXITSTATUS(result));
		/*
		 * Discard all output if command failed, for example command
		 * syntax failure may lead to garbage in stdout.
		 */
		buf[0] = '\0';
	}
	return strdup(buf);
}

static const char *host_get_crossystem_value(const char *name, char **value)
{
	char buf[COMMAND_BUFFER_SIZE];
	char *result = *value;
	if (result)
		return result;

	snprintf(buf, sizeof(buf), "crossystem %s", name);
	result = host_shell(buf);
	*value = result;
	Debug("%s: %s => %s\n", __FUNCTION__, name, result);
	return result;
}

static const char *host_get_mainfw_act(struct system_env *env)
{
	return host_get_crossystem_value("mainfw_act", &env->_mainfw_act);
}

static const char *host_get_tpm_fwver(struct system_env *env)
{
	return host_get_crossystem_value("tpm_fwver", &env->_tpm_fwver);
}

static int host_crossystem(const char *property, const char *value)
{
	char buf[COMMAND_BUFFER_SIZE];
	snprintf(buf, sizeof(buf), "crossystem %s=%s", property, value);
	Debug("%s: %s\n", __FUNCTION__, buf);
	return system(buf);
}

static const char *host_get_wpsw(struct system_env *env)
{
	if (env->_wpsw)
		return env->_wpsw;
	host_get_crossystem_value("wpsw_cur", &env->_wpsw);
	if (!*env->_wpsw) {
		free(env->_wpsw);
		host_get_crossystem_value("wpsw_boot", &env->_wpsw);
	}
	return env->_wpsw;
}

static int host_flashrom(enum flashrom_ops op, const char *image_path,
			 const char *programmer, int verbose,
			 const struct string_list *sections)
{
	char buf[COMMAND_BUFFER_SIZE];
	const char *op_cmd;
	int i;

	switch (op) {
	case FLASHROM_READ:
		op_cmd = "-r";
		assert(image_path);
		break;
	case FLASHROM_WRITE:
		op_cmd = "-w";
		assert(image_path);
		break;
	case FLASHROM_WP_STATUS:
		op_cmd = "--wp-status";
		assert(image_path == NULL);
		image_path = "";
		break;
	default:
		assert(0);
		return -1;
	}

	snprintf(buf, sizeof(buf), "flashrom %s %s -p %s", op_cmd,
		 image_path,
		 programmer);
	for (i = 0; sections && i < sections->size; i++) {
		char buf2[COMMAND_BUFFER_SIZE];
		snprintf(buf2, sizeof(buf2), " -i %s", sections->data[i]);
		/* TODO(hungte) make this safe. */
		strcat(buf, buf2);
	}

	if (verbose || debugging_enabled)
		printf("Executing: %s\n", buf);
	else if (op != FLASHROM_WP_STATUS)
		strcat(buf, " >/dev/null 2>&1");

	if (op != FLASHROM_WP_STATUS) {
		return system(buf);
	} else {
		char *result = host_shell(buf);
		// TODO(hungte) host_shell currently returns only 1 line.
		if (strstr(result, FLASHROM_WP_ENABLED))
			return WP_ENABLED;
		else if (strstr(result, FLASHROM_WP_DISABLED))
			return WP_DISABLED;
		else
			return -1;
	}
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

static const char *decide_rw_target(struct system_env *env,
				    enum target_type target)
{
	const char *act = env->get_mainfw_act(env);
	if (strcmp(act, FWACT_A) == 0)
		return target == TARGET_UPDATE ? RW_B : RW_A;
	else if (strcmp(act, FWACT_B) == 0)
		return target == TARGET_UPDATE? RW_A : RW_B;
	else
		return NULL;
}

static int set_try_cookies(struct updater_config *cfg, const char *try_next)
{
	/*
	 * If EC is available, we may need to do EC software sync which needs
	 * few more cycles.
	 */
	const char *tries = cfg->to.ec_image.data ? "8" : "6";
	RETURN_ON_FAILURE(cfg->env.crossystem("fw_try_next", try_next));
	RETURN_ON_FAILURE(cfg->env.crossystem("fw_try_count", tries));
	return 0;
}

static int write_firmware(struct updater_config *cfg,
			  struct firmware_image *image,
			  const struct string_list *sections)
{
	/* TODO(hungte) replace by mkstemp */
	const char *tmp_file = "/tmp/.fwupdate.write";
	FILE *fp = fopen(tmp_file, "wb");
	if (!fp)
		return -1;
	fwrite(image->data, image->size, 1, fp);
	fclose(fp);
	return cfg->env.flashrom(FLASHROM_WRITE, tmp_file, image->programmer, 1,
				 sections);
}

static int preserve_firmware_section(struct firmware_image *image_from,
				     struct firmware_image *image_to,
				     const char *section_name)
{
	struct firmware_section from, to;

	find_firmware_section(&from, image_from, section_name);
	find_firmware_section(&to, image_to, section_name);
	if (!from.data || !to.data)
		return -1;
	memmove(to.data, from.data, MIN(from.size, to.size));
	return 0;
}

static int preserve_gbb(struct firmware_image *image_from,
			struct firmware_image *image_to)
{
	int len;
	struct firmware_section from, to;
	GoogleBinaryBlockHeader *gbb_from, *gbb_to;

	find_firmware_section(&from, image_from, RO_GBB);
	find_firmware_section(&to, image_to, RO_GBB);
	gbb_from = (GoogleBinaryBlockHeader*)from.data;
	gbb_to = (GoogleBinaryBlockHeader*)to.data;

	if (!futil_valid_gbb_header(gbb_from, from.size, NULL) ||
	    !futil_valid_gbb_header(gbb_to, to.size, NULL))
		return -1;
	gbb_to->flags = gbb_from->flags;

	len = strlen(from.data + gbb_from->hwid_offset);
	if (len >= gbb_to->hwid_size)
		return -1;

	memset(to.data + gbb_to->hwid_offset, 0, gbb_to->hwid_size);
	/* Size for strcpy already ensured in previous checks. */
	strcpy(to.data + gbb_to->hwid_offset,
	       from.data + gbb_from->hwid_offset);
	return 0;
}

static int preserve_images(struct updater_config *cfg)
{
	int errcnt = 0;
	struct firmware_image *from = &cfg->from.image, *to = &cfg->to.image;
	errcnt += preserve_gbb(from, to);
	errcnt += preserve_firmware_section(from, to, RO_VPD);
	errcnt += preserve_firmware_section(from, to, RW_VPD);
	errcnt += preserve_firmware_section(from, to, RW_NVRAM);
	return errcnt;
}

static int compare_section(const struct firmware_section *a,
			   const struct firmware_section *b)
{
	if (a->size != b->size)
		return a->size - b->size;
	return memcmp(a->data, b->data, a->size);
}

static int images_have_same_section(struct firmware_image *image_from,
				    struct firmware_image *image_to,
				    const char *section_name)
{
	struct firmware_section from, to;

	find_firmware_section(&from, image_from, section_name);
	find_firmware_section(&to, image_from, section_name);
	return compare_section(&from, &to) == 0;
}

static int is_write_protection_enabled(struct updater_config *cfg)
{
	if (cfg->write_protection != WP_AUTODETECT)
		return cfg->write_protection;

	if (strcmp(cfg->env.get_wpsw(&cfg->env), WPSW_DISABLED) == 0) {
		cfg->write_protection = WP_DISABLED;
	} else {
		int r = cfg->env.flashrom(FLASHROM_WP_STATUS, NULL,
					  cfg->to.image.programmer, 1, NULL);
		if (r == WP_ENABLED || r == WP_DISABLED)
			cfg->write_protection = r;
		else
			cfg->write_protection = WP_ENABLED;
	}
	return cfg->write_protection;
}

static int is_compatible_platform(struct updater_config *cfg)
{
	int len;
	const char *from_dot = strchr(cfg->from.image.ro_version, '.'),
	           *to_dot = strchr(cfg->to.image.ro_version, '.');

	if (!from_dot || !to_dot) {
		Debug("%s: missing dot (from=%p, to=%p)\n", from_dot, to_dot);
		return 0;
	}
	len = from_dot - cfg->from.image.ro_version + 1;
	Debug("Platform: %*.*s\n", len, len, cfg->from.image.ro_version);
	return strncmp(cfg->from.image.ro_version, cfg->to.image.ro_version,
		       len) == 0;
}

static int is_compatible_keys(struct updater_config *cfg,
			      struct firmware_image *ro_image,
			      struct firmware_image *rw_image)
{
	/* TODO(hungte) Check if the rootkey and firmware key are OK. */
	return 1;
}

enum updater_error_codes {
	UPDATE_ERR_NONE,
	UPDATE_ERR_NO_IMAGE,
	UPDATE_ERR_SYSTEM_IMAGE,
	UPDATE_ERR_PLATFORM,
	UPDATE_ERR_TARGET,
	UPDATE_ERR_KEY,
	UPDATE_ERR_UNKNOWN,
};

static const char *updater_error_messages[] = {
	[UPDATE_ERR_NONE] = "None",
	[UPDATE_ERR_NO_IMAGE] = "No image to update; try specify with -i.",
	[UPDATE_ERR_SYSTEM_IMAGE] = "Cannot load system active firmware.",
	[UPDATE_ERR_PLATFORM] = "Your system platform is not compatible.",
	[UPDATE_ERR_TARGET] = "No valid RW target to update. Abort.",
	[UPDATE_ERR_KEY] = "Keys in RW may be not compatible.",
	[UPDATE_ERR_UNKNOWN] = "Unknown error.",
};

static int update_firmware(struct updater_config *cfg)
{
	struct string_list sections = {0};
	struct firmware_image *from_image = &cfg->from.image,
			      *to_image = &cfg->to.image;
	int wp_enabled;
	if (!to_image->data)
		return UPDATE_ERR_NONE;

	printf("Target image: %s (RO:%s, RW/A:%s, RW/B:%s)\n",
	       to_image->file_name, to_image->ro_version,
	       to_image->rw_version_a, to_image->rw_version_b);

	if (to_image->data && !from_image->data) {
		/*
		 * TODO(hungte) Read only RO_SECTION, VBLOCK_A, VBLOCK_B,
		 * RO_VPD, RW_VPD, RW_NVRAM, RW_LEGACY.
		 */
		printf("Loading current system firmware...\n");
		if (load_system_image(cfg, from_image) != 0)
			return UPDATE_ERR_SYSTEM_IMAGE;
	}
	printf("Current system: %s (RO:%s, RW/A:%s, RW/B:%s)\n",
	       from_image->file_name, from_image->ro_version,
	       from_image->rw_version_a, from_image->rw_version_b);

	if (!is_compatible_platform(cfg))
		return UPDATE_ERR_PLATFORM;

	wp_enabled = is_write_protection_enabled(cfg);
	while (cfg->try_update) {
		const char *target;

		preserve_gbb(from_image, to_image);
		if (!wp_enabled &&
		    !images_have_same_section(from_image, to_image, RO_ALL)) {
			printf("WP disabled and RO changed. Do full update.\n");
			break;
		}

		if (!is_compatible_keys(cfg, from_image, to_image)) {
			printf("New RW may be not compatible.\n");
			return -1;
		}

		/* TODO(hungte): Support vboot1. */
		target = decide_rw_target(&cfg->env, TARGET_SELF);
		if (target == NULL) {
			return UPDATE_ERR_TARGET;
		}
		if (images_have_same_section(from_image, to_image, target)) {
			printf("No need to update.\n");
			return UPDATE_ERR_NONE;
		}

		target = decide_rw_target(&cfg->env, TARGET_UPDATE);
		append_string_list(&sections, target);
		write_firmware(cfg, to_image, &sections);
		set_try_cookies(cfg, target);
		free_string_list(&sections);
		return UPDATE_ERR_NONE;
	}

	if (wp_enabled) {
		printf("Updating %s, %s, and %s.\n", RW_A, RW_B, RW_SHARED);

		if (!is_compatible_keys(cfg, from_image, to_image))
			return UPDATE_ERR_KEY;

		append_string_list(&sections, RW_A);
		append_string_list(&sections, RW_B);
		append_string_list(&sections, RW_SHARED);
		write_firmware(cfg, to_image, &sections);

		free_string_list(&sections);
		append_string_list(&sections, RW_LEGACY);
		write_firmware(cfg, to_image, &sections);
	} else {
		printf("Updating entire firmware images.\n");
		preserve_images(cfg);

		if (!is_compatible_keys(cfg, to_image, to_image))
			return UPDATE_ERR_KEY;

		/* FMAP may be different so we should just update all. */
		write_firmware(cfg, to_image, NULL);
		if (cfg->to.ec_image.data)
			write_firmware(cfg, &cfg->to.ec_image, NULL);
		if (cfg->to.pd_image.data)
			write_firmware(cfg, &cfg->to.pd_image, NULL);
	}
	free_string_list(&sections);
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
	{"config", 1, NULL, 'c'},
	{"image", 1, NULL, 'i'},
	{"ec_image", 1, NULL, 'e'},
	{"pd_image", 1, NULL, 'P'},
	{"try", 0, NULL, 't'},
	{"wp", 1, NULL, 'W'},
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static const char *short_opts = "c:i:e:t";
static int errorcnt = 0;

static void print_help(int argc, char *argv[])
{
	printf("\n"
		"Usage:  " MYNAME " %s [OPTIONS]\n"
		"\n"
		"-c, --config=FILE  \tRead JSON config file\n"
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
			.get_mainfw_act = host_get_mainfw_act,
			.get_tpm_fwver = host_get_tpm_fwver,
			.get_wpsw = host_get_wpsw,
			.flashrom = host_flashrom,
			.crossystem = host_crossystem,
		},
		.try_update = 0,
		.write_protection = WP_AUTODETECT,

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
				Error("Unrecognized option (possibly '%s'\n",
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
