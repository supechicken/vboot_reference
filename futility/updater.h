/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * A reference implementation for AP (and supporting images) firmware updater.
 */

#include <stdint.h>

/* For FmapHeader */
#include "fmap.h"
#include "futility.h"

#define DEBUG(format, ...) Debug("%s: " format "\n", __FUNCTION__,##__VA_ARGS__)
#define ERROR(format, ...) Error("%s: " format "\n", __FUNCTION__,##__VA_ARGS__)

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

struct system_property {
	int (*getter)();
	int value;
	int initialized;
};

enum system_property_type {
	SYS_PROP_MAINFW_ACT,
	SYS_PROP_TPM_FWVER,
	SYS_PROP_FW_VBOOT2,
	SYS_PROP_PLATFORM_VER,
	SYS_PROP_WP_HW,
	SYS_PROP_WP_SW,
	SYS_PROP_MAX
};

struct updater_config;
struct quirk_entry {
	const char *name;
	const char *help;
	int (*apply)(struct updater_config *cfg);
	int value;
};

enum quirk_types {
	QUIRK_ENLARGE_IMAGE,
	QUIRK_UNLOCK_ME_FOR_UPDATE,
	QUIRK_MIN_PLATFORM_VERSION,
	QUIRK_MAX,
};

struct tempfile {
	char *filepath;
	struct tempfile *next;
};

struct updater_config {
	struct firmware_image image, image_current;
	struct firmware_image ec_image, pd_image;
	struct system_property system_properties[SYS_PROP_MAX];
	struct quirk_entry quirks[QUIRK_MAX];
	int try_update;
	int force_update;
	int legacy_update;
	const char *emulation;
	struct tempfile *tempfiles;
};

enum updater_error_codes {
	UPDATE_ERR_DONE,
	UPDATE_ERR_NEED_RO_UPDATE,
	UPDATE_ERR_NO_IMAGE,
	UPDATE_ERR_SYSTEM_IMAGE,
	UPDATE_ERR_INVALID_IMAGE,
	UPDATE_ERR_SET_COOKIES,
	UPDATE_ERR_WRITE_FIRMWARE,
	UPDATE_ERR_PLATFORM,
	UPDATE_ERR_TARGET,
	UPDATE_ERR_ROOT_KEY,
	UPDATE_ERR_TPM_ROLLBACK,
	UPDATE_ERR_UNKNOWN,
};

/* Messages for decoding enum updater_error_codes. */
extern const char * const updater_error_messages[];

/* Updater functions. */
void updater_init_config(struct updater_config *cfg);
void updater_dispose_config(struct updater_config *cfg);
enum updater_error_codes update_firmware(struct updater_config *cfg);

int updater_setup_config(struct updater_config *cfg,
			 const char *image,
			 const char *ec_image,
			 const char *pd_image,
			 const char *quirks,
			 const char *mode,
			 const char *programmer,
			 const char *emulation,
			 const char *sys_props,
			 const char *write_protection,
			 int is_factory,
			 int list_quirks);
