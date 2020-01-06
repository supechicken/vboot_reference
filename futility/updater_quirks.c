/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The board-specific quirks needed by firmware updater.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "crossystem.h"
#include "futility.h"
#include "host_misc.h"
#include "updater.h"
#include "util_misc.h"

struct quirks_record {
	const char * const match;
	const char * const quirks;
};

static const struct quirks_record quirks_records[] = {
	{ .match = "Google_Whirlwind.", .quirks = "enlarge_image" },
	{ .match = "Google_Arkham.", .quirks = "enlarge_image" },
	{ .match = "Google_Storm.", .quirks = "enlarge_image" },
	{ .match = "Google_Gale.", .quirks = "enlarge_image" },

	{ .match = "Google_Chell.", .quirks = "unlock_me_for_update" },
	{ .match = "Google_Lars.", .quirks = "unlock_me_for_update" },
	{ .match = "Google_Sentry.", .quirks = "unlock_me_for_update" },
	{ .match = "Google_Asuka.", .quirks = "unlock_me_for_update" },
	{ .match = "Google_Caroline.", .quirks = "unlock_me_for_update" },
	{ .match = "Google_Cave.", .quirks = "unlock_me_for_update" },

	{ .match = "Google_Eve.",
	  .quirks = "unlock_me_for_update,eve_smm_store" },

	{ .match = "Google_Sarien.",
	  .quirks = "unlock_wilco_me_for_update" },
	{ .match = "Google_Arcada.",
	  .quirks = "unlock_wilco_me_for_update" },

	{ .match = "Google_Poppy.", .quirks = "min_platform_version=6" },
	{ .match = "Google_Scarlet.", .quirks = "min_platform_version=1" },

        /* Legacy white label units. */
        { .match = "Google_Enguarde.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Expresso.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Hana.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Veyron_Jaq.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Veyron_Jerry.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Veyron_Mighty.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Reks.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Relm.", .quirks = "allow_empty_wltag" },
        { .match = "Google_Wizpig.", .quirks = "allow_empty_wltag" },

	{ .match = "Google_Phaser.", .quirks = "allow_dual_root_key"},
};

/* Preserves meta data and reload image contents from given file path. */
static int reload_firmware_image(const char *file_path,
				 struct firmware_image *image)
{
	free_firmware_image(image);
	return load_firmware_image(image, file_path, NULL);
}

/*
 * Returns True if the system has EC software sync enabled.
 */
static int is_ec_software_sync_enabled(struct updater_config *cfg)
{
	const struct vb2_gbb_header *gbb;

	/* Check if current system has disabled software sync or no support. */
	if (!(VbGetSystemPropertyInt("vdat_flags") & VBSD_EC_SOFTWARE_SYNC)) {
		INFO("EC Software Sync is not available.\n");
		return 0;
	}

	/* Check if the system has been updated to disable software sync. */
	gbb = find_gbb(&cfg->image);
	if (!gbb) {
		WARN("Invalid AP firmware image.\n");
		return 0;
	}
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC) {
		INFO("EC Software Sync will be disabled in next boot.\n");
		return 0;
	}
	return 1;
}

/*
 * Schedules an EC RO software sync (in next boot) if applicable.
 */
static int ec_ro_software_sync(struct updater_config *cfg)
{
	const char *ec_ro_path;
	uint8_t *ec_ro_data;
	uint32_t ec_ro_len;
	int is_same_ec_ro;
	struct firmware_section ec_ro_sec;
	const char *tmp_path = get_firmware_image_temp_file(
			&cfg->image, &cfg->tempfiles);

	if (!tmp_path)
		return 1;
	find_firmware_section(&ec_ro_sec, &cfg->ec_image, "EC_RO");
	if (!ec_ro_sec.data || !ec_ro_sec.size) {
		ERROR("EC image has invalid section '%s'.\n", "EC_RO");
		return 1;
	}
	ec_ro_path = cbfs_extract_file(tmp_path, FMAP_RO_SECTION, "ecro",
				       &cfg->tempfiles);
	if (!ec_ro_path ||
	    !cbfs_file_exists(tmp_path, FMAP_RO_SECTION, "ecro.hash")) {
		INFO("No valid EC RO for software sync in AP firmware.\n");
		return 1;
	}
	if (vb2_read_file(ec_ro_path, &ec_ro_data, &ec_ro_len) != VB2_SUCCESS) {
		ERROR("Failed to read EC RO.\n");
		return 1;
	}

	is_same_ec_ro = (ec_ro_len <= ec_ro_sec.size &&
			 memcmp(ec_ro_sec.data, ec_ro_data, ec_ro_len) == 0);
	free(ec_ro_data);

	if (!is_same_ec_ro) {
		/* TODO(hungte) If change AP RO is not a problem (hash will be
		 * different, which may be a problem to factory and HWID), or if
		 * we can be be sure this is for developers, extract EC RO and
		 * update AP RO CBFS to trigger EC RO sync with new EC.
		 */
		ERROR("The EC RO contents specified from AP (--image) and EC "
		      "(--ec_image) firmware images are different, cannot "
		      "update by EC RO software sync.\n");
		return 1;
	}
	VbSetSystemPropertyInt("try_ro_sync", 1);
	return 0;
}

/*
 * Returns True if EC is running in RW.
 */
static int is_ec_in_rw(void)
{
	char buf[VB_MAX_STRING_PROPERTY];
	return (VbGetSystemPropertyString("ecfw_act", buf, sizeof(buf)) &&
		strcasecmp(buf, "RW") == 0);
}

/*
 * Quirk to enlarge a firmware image to match flash size. This is needed by
 * devices using multiple SPI flash with different sizes, for example 8M and
 * 16M. The image_to will be padded with 0xFF using the size of image_from.
 * Returns 0 on success, otherwise failure.
 */
static int quirk_enlarge_image(struct updater_config *cfg)
{
	struct firmware_image *image_from = &cfg->image_current,
			      *image_to = &cfg->image;
	const char *tmp_path;
	size_t to_write;
	FILE *fp;

	if (image_from->size <= image_to->size)
		return 0;

	tmp_path = get_firmware_image_temp_file(image_to, &cfg->tempfiles);
	if (!tmp_path)
		return -1;

	VB2_DEBUG("Resize image from %u to %u.\n",
		  image_to->size, image_from->size);
	to_write = image_from->size - image_to->size;
	fp = fopen(tmp_path, "ab");
	if (!fp) {
		ERROR("Cannot open temporary file %s.\n", tmp_path);
		return -1;
	}
	while (to_write-- > 0)
		fputc('\xff', fp);
	fclose(fp);
	return reload_firmware_image(tmp_path, image_to);
}

/*
 * Quirk to unlock a firmware image with SI_ME (management engine) when updating
 * so the system has a chance to make sure SI_ME won't be corrupted on next boot
 * before locking the Flash Master values in SI_DESC.
 * Returns 0 on success, otherwise failure.
 */
static int quirk_unlock_me_for_update(struct updater_config *cfg)
{
	struct firmware_section section;
	struct firmware_image *image_to = &cfg->image;
	const int flash_master_offset = 128;
	const uint8_t flash_master[] = {
		0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff,
		0xff, 0xff
	};

	find_firmware_section(&section, image_to, FMAP_SI_DESC);
	if (section.size < flash_master_offset + ARRAY_SIZE(flash_master))
		return 0;
	if (memcmp(section.data + flash_master_offset, flash_master,
		   ARRAY_SIZE(flash_master)) == 0) {
		VB2_DEBUG("Target ME not locked.\n");
		return 0;
	}
	/*
	 * b/35568719: We should only update with unlocked ME and let
	 * board-postinst lock it.
	 */
	INFO("Changed Flash Master Values to unlocked.\n");
	memcpy(section.data + flash_master_offset, flash_master,
	       ARRAY_SIZE(flash_master));
	return 0;
}

/*
 * Quirk to unlock a firmware image with SI_ME (management engine) when updating
 * so the system has a chance to make sure SI_ME won't be corrupted on next boot
 * before locking the Flash Master values in SI_DESC.
 * Returns 0 on success, otherwise failure.
 */
static int quirk_unlock_wilco_me_for_update(struct updater_config *cfg)
{
	struct firmware_section section;
	struct firmware_image *image_to = &cfg->image;
	const int flash_master_offset = 128;
	const uint8_t flash_master[] = {
		0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff
	};

	find_firmware_section(&section, image_to, FMAP_SI_DESC);
	if (section.size < flash_master_offset + ARRAY_SIZE(flash_master))
		return 0;
	if (memcmp(section.data + flash_master_offset, flash_master,
		   ARRAY_SIZE(flash_master)) == 0) {
		VB2_DEBUG("Target ME not locked.\n");
		return 0;
	}
	INFO("Changed Flash Master Values to unlocked.\n");
	memcpy(section.data + flash_master_offset, flash_master,
	       ARRAY_SIZE(flash_master));
	return 0;
}

/*
 * Checks and returns 0 if the platform version of current system is larger
 * or equal to given number, otherwise non-zero.
 */
static int quirk_min_platform_version(struct updater_config *cfg)
{
	int min_version = get_config_quirk(QUIRK_MIN_PLATFORM_VERSION, cfg);
	int platform_version = get_system_property(SYS_PROP_PLATFORM_VER, cfg);

	VB2_DEBUG("Minimum required version=%d, current platform version=%d\n",
		  min_version, platform_version);

	if (platform_version >= min_version)
		return 0;
	ERROR("Need platform version >= %d (current is %d). "
	      "This firmware will only run on newer systems.\n",
	      min_version, platform_version);
	return -1;
}

/*
 * Quirk to help preserving SMM store on devices without a dedicated "SMMSTORE"
 * FMAP section. These devices will store "smm_store" file in same CBFS where
 * the legacy boot loader lives (i.e, FMAP RW_LEGACY).
 * Note this currently has dependency on external program "cbstool".
 * Returns 0 if the SMM store is properly preserved, or if the system is not
 * available to do that (problem in cbfstool, or no "smm_store" in current
 * system firmware). Otherwise non-zero as failure.
 */
static int quirk_eve_smm_store(struct updater_config *cfg)
{
	const char *smm_store_name = "smm_store";
	const char *old_store;
	char *command;
	const char *temp_image = get_firmware_image_temp_file(
			&cfg->image_current, &cfg->tempfiles);

	if (!temp_image)
		return -1;

	old_store = cbfs_extract_file(temp_image, FMAP_RW_LEGACY,
				      smm_store_name, &cfg->tempfiles);
	if (!old_store) {
		VB2_DEBUG("cbfstool failure or SMM store not available. "
			  "Don't preserve.\n");
		return 0;
	}

	/* Reuse temp_image */
	temp_image = get_firmware_image_temp_file(&cfg->image, &cfg->tempfiles);
	if (!temp_image)
		return -1;

	/* crosreview.com/1165109: The offset is fixed at 0x1bf000. */
	ASPRINTF(&command,
		 "cbfstool \"%s\" remove -r %s -n \"%s\" 2>/dev/null; "
		 "cbfstool \"%s\" add -r %s -n \"%s\" -f \"%s\" "
		 " -t raw -b 0x1bf000", temp_image, FMAP_RW_LEGACY,
		 smm_store_name, temp_image, FMAP_RW_LEGACY,
		 smm_store_name, old_store);
	free(host_shell(command));
	free(command);

	return reload_firmware_image(temp_image, &cfg->image);
}

/*
 * Update EC (RO+RW) in most reliable way.
 *
 * Some EC will reset TCPC when doing sysjump, and will make rootfs unavailable
 * if the system was boot from USB, or other unexpected issues even if the
 * system was boot from internal disk. To prevent that, try to partial update
 * only RO and expect EC software sync to update RW later, or perform EC RO
 * software sync.
 *
 * Returns:
 *  EC_RECOVERY_FULL to indicate a full recovery is needed.
 *  EC_RECOVERY_RO to indicate partial update (WP_RO) is needed.
 *  EC_RECOVERY_DONE to indicate EC RO software sync is applied.
 *  Other values to report failure.
 */
static int quirk_ec_partial_recovery(struct updater_config *cfg)
{
	/*
	 * http://crbug.com/1024401: Some EC needs extra header outside EC_RO so
	 * we have to update whole WP_RO, not just EC_RO.
	 */
	const char *ec_ro = "WP_RO";
	struct firmware_image *ec_image = &cfg->ec_image;

	int do_partial = get_config_quirk(QUIRK_EC_PARTIAL_RECOVERY, cfg);
	if (do_partial == -1) {
		char arch[VB_MAX_STRING_PROPERTY];
		/*
		 * Don't do partial update if can't decide arch (usually implies
		 * running outside DUT).
		 */
		do_partial = 0;
		if (VbGetSystemPropertyString("arch", arch, sizeof(arch)) > 0) {
			/* By default disabled for x86, otherwise enabled. */
			do_partial = !!strcmp(arch, "x86");
		}
	}

	if (!do_partial) {
		return EC_RECOVERY_FULL;
	} else if (!firmware_section_exists(ec_image, ec_ro)) {
		INFO("EC image does not have section '%s'.\n", ec_ro);
		/* Need full update. */
	} else if (!is_ec_software_sync_enabled(cfg)) {
		/* Message already printed, need full update. */
	} else if (is_ec_in_rw()) {
		WARN("EC Software Sync detected, will only update EC RO. "
		     "The contents in EC RW will be updated after reboot.\n");
		return EC_RECOVERY_RO;
	} else if (ec_ro_software_sync(cfg) == 0) {
		INFO("EC RO and RW should be updated after reboot.\n");
		return EC_RECOVERY_DONE;
	}

	WARN("Update EC RO+RW and may cause unexpected error later. "
	     "See http://crbug.com/782427#c4 for more information.\n");
	return EC_RECOVERY_FULL;
}

/*
 * This function will first judge whether the device is phaser360
 * device with dopefish root key. If it is and its whitelabe_tag
 * is null, then we need re-patch image with dopefish rootkey and
 * vblock_a/vblock_b. When re-patch is neccessary, this function
 * will return 1.
 */
static int is_dual_root_key_model(struct updater_config *cfg)
{
	const char * const PHASER360 = "phaser360";
	const char * const DOPEFISH_ROOT_KEY = "9a1f2cc319e2f2e61237dc51125e35ddd4d20984";
	const char * const VPD_WHITELABEL_TAG = "whitelabel_tag";

	char *sys_model_name = NULL;
	struct firmware_image *image_from = &cfg->image_current;
	const struct vb2_gbb_header *gbb = NULL;
	const struct vb2_packed_key *rootkey = NULL;

	const char *tmp_image = NULL;
	char *wl_tag = NULL;

	/* phaser and phaser360 use the same firmware */
	sys_model_name = host_shell("mosys platform model");
	INFO("System model name: '%s'\n", sys_model_name);
	if (strcmp(sys_model_name, PHASER360) != 0) {
		free(sys_model_name);
		return 0;
	}
	free(sys_model_name);

	gbb = find_gbb(image_from);
	if (!gbb) {
		WARN("No system gbb found in system image");
		return 0;
	}

	rootkey = get_rootkey(gbb);
	if (!rootkey) {
		WARN("No system rootkey found in system image");
		return 0;
	}

	if (strcmp(packed_key_sha1_string(rootkey), DOPEFISH_ROOT_KEY) != 0) {
		INFO("Not a phaser360 with dopefish root key");
		return 0;
	}

	/*
	 * now the device is phaser360 with a dopefish key.
	 * we need see wltag is null or not, if null then re-patch rootkey/vblocks.
	 * otherwise it is a real dopefish device, nothing to do.
	 */
	tmp_image = get_firmware_image_temp_file(image_from, &cfg->tempfiles);
	if (!tmp_image)
		return 0;

	wl_tag = vpd_get_value(tmp_image, VPD_WHITELABEL_TAG);
	if (wl_tag) {
		WARN("Device is a real dopefish model, wl(%s)", wl_tag);
		free(wl_tag);
		return 0;
	}

	return 1;
}

/*
 * Quirk to allow one dev model has two differents root keys.
 * Some devices can use either one of two root keys. this
 * function will select proper one to use when do update.
 */
static int quirk_dual_root_key(struct updater_config *cfg)
{
	const char * const PHASER360 = "phaser360";
	const char * const DOPEFISH_WL_TAG = "dopefish";

	struct archive *archive = cfg->archive;
	struct model_config model = {0};
	char *sig_id = NULL;
	int errcnt = 0;

	if (!is_dual_root_key_model(cfg)) {
		INFO("Not a dual root key model");
		return 0;
	}

	ASPRINTF(&sig_id, "%s-%s", PHASER360, DOPEFISH_WL_TAG);

	find_patches_for_model(&model, archive, sig_id);
	INFO("found rootkey (%s), vblock_a (%s), vblock_b (%s)",
		model.patches.rootkey, model.patches.vblock_a, model.patches.vblock_b);
	if ( !model.patches.rootkey || !model.patches.vblock_a ||
					!model.patches.vblock_b) {
		WARN("can't find rootkey, vblock_a or vblock_b image");
		errcnt += -1;
	}

	errcnt += patch_image_by_model(&cfg->image, &model, archive);

	if (errcnt < 0) {
		WARN("failed to patch image");
	}

	free(sig_id);
	free(model.patches.rootkey);
	free(model.patches.vblock_a);
	free(model.patches.vblock_b);

	return errcnt;
}

/*
 * Registers known quirks to a updater_config object.
 */
void updater_register_quirks(struct updater_config *cfg)
{
	struct quirk_entry *quirks;

	assert(ARRAY_SIZE(cfg->quirks) == QUIRK_MAX);
	quirks = &cfg->quirks[QUIRK_ENLARGE_IMAGE];
	quirks->name = "enlarge_image";
	quirks->help = "Enlarge firmware image by flash size.";
	quirks->apply = quirk_enlarge_image;

	quirks = &cfg->quirks[QUIRK_MIN_PLATFORM_VERSION];
	quirks->name = "min_platform_version";
	quirks->help = "Minimum compatible platform version "
			"(also known as Board ID version).";
	quirks->apply = quirk_min_platform_version;

	quirks = &cfg->quirks[QUIRK_UNLOCK_WILCO_ME_FOR_UPDATE];
	quirks->name = "unlock_wilco_me_for_update";
	quirks->help = "Unlock ME for safe lockdown.";
	quirks->apply = quirk_unlock_wilco_me_for_update;

	quirks = &cfg->quirks[QUIRK_UNLOCK_ME_FOR_UPDATE];
	quirks->name = "unlock_me_for_update";
	quirks->help = "b/35568719; only lock management engine in "
			"board-postinst.";
	quirks->apply = quirk_unlock_me_for_update;

	quirks = &cfg->quirks[QUIRK_EVE_SMM_STORE];
	quirks->name = "eve_smm_store";
	quirks->help = "b/70682365; preserve UEFI SMM store without "
		       "dedicated FMAP section.";
	quirks->apply = quirk_eve_smm_store;

	quirks = &cfg->quirks[QUIRK_ALLOW_EMPTY_WLTAG];
	quirks->name = "allow_empty_wltag";
	quirks->help = "chromium/906962; allow devices without white label "
		       "tags set to use default keys.";
	quirks->apply = NULL;  /* Simple config. */

	quirks = &cfg->quirks[QUIRK_EC_PARTIAL_RECOVERY];
	quirks->name = "ec_partial_recovery";
	quirks->help = "chromium/1024401; recover EC by partial RO update.";
	quirks->apply = quirk_ec_partial_recovery;
	quirks->value = -1;  /* Decide at runtime. */

	quirks = &cfg->quirks[QUIRK_DUAL_ROOT_KEY];
	quirks->name = "allow_dual_root_key";
	quirks->help = "b/146876241; allow devices with one of two "
			"root keys.";
	quirks->apply = quirk_dual_root_key;
}

/*
 * Gets the default quirk config string for target image.
 * Returns a string (in same format as --quirks) to load or NULL if no quirks.
 */
const char * const updater_get_default_quirks(struct updater_config *cfg)
{
	const char *pattern = cfg->image.ro_version;
	int i;

	if (!pattern) {
		VB2_DEBUG("Cannot identify system for default quirks.\n");
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(quirks_records); i++) {
		const struct quirks_record *r = &quirks_records[i];
		if (strncmp(r->match, pattern, strlen(r->match)) != 0)
		    continue;
		VB2_DEBUG("Found system default quirks: %s\n", r->quirks);
		return r->quirks;
	}
	return NULL;
}
