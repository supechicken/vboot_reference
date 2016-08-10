/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Display functions used in kernel selection.
 */

#include "sysincludes.h"
#include "2sysincludes.h"

#include "2common.h"
#include "2sha.h"
#include "gbb_access.h"
#include "gbb_header.h"
#include "region.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_nvstorage.h"

static uint32_t disp_current_screen = VB_SCREEN_BLANK;

__attribute__((weak))
VbError_t VbExGetLocalizationCount(uint32_t *count) {
	return VBERROR_UNKNOWN;
}

VbError_t VbGetLocalizationCount(uint32_t *count)
{
	return VbExGetLocalizationCount(count);
}

VbError_t VbDisplayScreen(uint32_t screen, int force, VbNvContext *vncptr)
{
	uint32_t locale;
	VbError_t rv;

	/* If requested screen is the same as the current one, we're done. */
	if (disp_current_screen == screen && !force)
		return VBERROR_SUCCESS;

	/* Read the locale last saved */
	VbNvGet(vncptr, VBNV_LOCALIZATION_INDEX, &locale);

	rv = VbExDisplayScreen(screen, locale);
	if (rv == VBERROR_SUCCESS)
		/* Keep track of the currently displayed screen */
		disp_current_screen = screen;

	return rv;
}

static void Uint8ToString(char *buf, uint8_t val)
{
	const char *trans = "0123456789abcdef";
	*buf++ = trans[val >> 4];
	*buf = trans[val & 0xF];
}

static void FillInSha1Sum(char *outbuf, VbPublicKey *key)
{
	uint8_t *buf = ((uint8_t *)key) + key->key_offset;
	uint64_t buflen = key->key_size;
	uint8_t digest[VB2_SHA1_DIGEST_SIZE];
	int i;

	vb2_digest_buffer(buf, buflen, VB2_HASH_SHA1, digest, sizeof(digest));
	for (i = 0; i < sizeof(digest); i++) {
		Uint8ToString(outbuf, digest[i]);
		outbuf += 2;
	}
	*outbuf = '\0';
}

const char *RecoveryReasonString(uint8_t code)
{
	switch(code) {
	case VBNV_RECOVERY_NOT_REQUESTED:
		return "Recovery not requested";
	case VBNV_RECOVERY_LEGACY:
		return "Recovery requested from legacy utility";
	case VBNV_RECOVERY_RO_MANUAL:
		return "recovery button pressed";
	case VBNV_RECOVERY_RO_INVALID_RW:
		return "RW firmware failed signature check";
	case VBNV_RECOVERY_RO_S3_RESUME:
		return "S3 resume failed";
	case VBNV_RECOVERY_DEP_RO_TPM_ERROR:
		return "TPM error in read-only firmware";
	case VBNV_RECOVERY_RO_SHARED_DATA:
		return "Shared data error in read-only firmware";
	case VBNV_RECOVERY_RO_TEST_S3:
		return "Test error from S3Resume()";
	case VBNV_RECOVERY_RO_TEST_LFS:
		return "Test error from LoadFirmwareSetup()";
	case VBNV_RECOVERY_RO_TEST_LF:
		return "Test error from LoadFirmware()";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_NOT_DONE:
		return "RW firmware check not done";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_DEV_MISMATCH:
	  return "RW firmware developer flag mismatch";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_REC_MISMATCH:
		return "RW firmware recovery flag mismatch";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN +
		VBSD_LF_CHECK_VERIFY_KEYBLOCK:
		return "RW firmware unable to verify key block";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_KEY_ROLLBACK:
		return "RW firmware key version rollback detected";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN +
		VBSD_LF_CHECK_DATA_KEY_PARSE:
		return "RW firmware unable to parse data key";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN +
		VBSD_LF_CHECK_VERIFY_PREAMBLE:
		return "RW firmware unable to verify preamble";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_FW_ROLLBACK:
		return "RW firmware version rollback detected";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_GET_FW_BODY:
		return "RW firmware unable to get firmware body";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN +
		VBSD_LF_CHECK_HASH_WRONG_SIZE:
		return "RW firmware hash is wrong size";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_VERIFY_BODY:
		return "RW firmware unable to verify firmware body";
	case VBNV_RECOVERY_RO_INVALID_RW_CHECK_MIN + VBSD_LF_CHECK_NO_RO_NORMAL:
		return "RW firmware read-only normal path is not supported";
	case VBNV_RECOVERY_RO_FIRMWARE:
		return "Firmware problem outside of verified boot";
	case VBNV_RECOVERY_RO_TPM_REBOOT:
		return "TPM requires a system reboot (should be transient)";
	case VBNV_RECOVERY_EC_SOFTWARE_SYNC:
		return "EC software sync error";
	case VBNV_RECOVERY_EC_UNKNOWN_IMAGE:
		return "EC software sync unable to determine active EC image";
	case VBNV_RECOVERY_DEP_EC_HASH:
		return "EC software sync error obtaining EC image hash";
	case VBNV_RECOVERY_EC_EXPECTED_IMAGE:
		return "EC software sync error "
			"obtaining expected EC image from BIOS";
	case VBNV_RECOVERY_EC_EXPECTED_HASH:
		return "EC software sync error "
			"obtaining expected EC hash from BIOS";
	case VBNV_RECOVERY_EC_HASH_MISMATCH:
		return "EC software sync error "
			"comparing expected EC hash and image";
	case VBNV_RECOVERY_EC_UPDATE:
		return "EC software sync error updating EC";
	case VBNV_RECOVERY_EC_JUMP_RW:
		return "EC software sync unable to jump to EC-RW";
	case VBNV_RECOVERY_EC_PROTECT:
		return "EC software sync protection error";
	case VBNV_RECOVERY_VB2_SECDATA_INIT:
		return "Secure NVRAM (TPM) initialization error";
	case VBNV_RECOVERY_VB2_GBB_HEADER:
		return "Error parsing GBB header";
	case VBNV_RECOVERY_VB2_TPM_CLEAR_OWNER:
		return "Error trying to clear TPM owner";
	case VBNV_RECOVERY_VB2_DEV_SWITCH:
		return "Error reading or updating developer switch";
	case VBNV_RECOVERY_VB2_FW_SLOT:
		return "Error selecting RW firmware slot";
	case VBNV_RECOVERY_RO_UNSPECIFIED:
		return "Unspecified/unknown error in RO firmware";
	case VBNV_RECOVERY_RW_DEV_SCREEN:
		return "User requested recovery from dev-mode warning screen";
	case VBNV_RECOVERY_RW_NO_OS:
		return "No OS kernel detected (or kernel rollback attempt?)";
	case VBNV_RECOVERY_RW_INVALID_OS:
		return "OS kernel failed signature check";
	case VBNV_RECOVERY_DEP_RW_TPM_ERROR:
		return "TPM error in rewritable firmware";
	case VBNV_RECOVERY_RW_DEV_MISMATCH:
		return "RW firmware in dev mode, but dev switch is off";
	case VBNV_RECOVERY_RW_SHARED_DATA:
		return "Shared data error in rewritable firmware";
	case VBNV_RECOVERY_RW_TEST_LK:
		return "Test error from LoadKernel()";
	case VBNV_RECOVERY_DEP_RW_NO_DISK:
		return "No bootable disk found";
	case VBNV_RECOVERY_TPM_E_FAIL:
		return "TPM error that was not fixed by reboot";
	case VBNV_RECOVERY_RO_TPM_S_ERROR:
		return "TPM setup error in read-only firmware";
	case VBNV_RECOVERY_RO_TPM_W_ERROR:
		return "TPM write error in read-only firmware";
	case VBNV_RECOVERY_RO_TPM_L_ERROR:
		return "TPM lock error in read-only firmware";
	case VBNV_RECOVERY_RO_TPM_U_ERROR:
		return "TPM update error in read-only firmware";
	case VBNV_RECOVERY_RW_TPM_R_ERROR:
		return "TPM read error in rewritable firmware";
	case VBNV_RECOVERY_RW_TPM_W_ERROR:
		return "TPM write error in rewritable firmware";
	case VBNV_RECOVERY_RW_TPM_L_ERROR:
		return "TPM lock error in rewritable firmware";
	case VBNV_RECOVERY_EC_HASH_FAILED:
		return "EC software sync unable to get EC image hash";
	case VBNV_RECOVERY_EC_HASH_SIZE:
		return "EC software sync invalid image hash size";
	case VBNV_RECOVERY_LK_UNSPECIFIED:
		return "Unspecified error while trying to load kernel";
	case VBNV_RECOVERY_RW_NO_DISK:
		return "No bootable storage device in system";
	case VBNV_RECOVERY_RW_NO_KERNEL:
		return "No bootable kernel found on disk";
	case VBNV_RECOVERY_RW_BCB_ERROR:
		return "BCB partition error on disk";
	case VBNV_RECOVERY_FW_FASTBOOT:
		return "Fastboot-mode requested in firmware";
	case VBNV_RECOVERY_RW_UNSPECIFIED:
		return "Unspecified/unknown error in RW firmware";
	case VBNV_RECOVERY_KE_DM_VERITY:
		return "DM-verity error";
	case VBNV_RECOVERY_KE_UNSPECIFIED:
		return "Unspecified/unknown error in kernel";
	case VBNV_RECOVERY_US_TEST:
		return "Recovery mode test from user-mode";
	case VBNV_RECOVERY_BCB_USER_MODE:
		return "User-mode requested recovery via BCB";
	case VBNV_RECOVERY_US_FASTBOOT:
		return "User-mode requested fastboot mode";
	case VBNV_RECOVERY_US_UNSPECIFIED:
		return "Unspecified/unknown error in user-mode";
	}
	return "We have no idea what this means";
}

#define DEBUG_INFO_SIZE 512

VbError_t VbDisplayDebugInfo(VbCommonParams *cparams, VbNvContext *vncptr)
{
	VbSharedDataHeader *shared =
		(VbSharedDataHeader *)cparams->shared_data_blob;
	GoogleBinaryBlockHeader *gbb = cparams->gbb;
	char buf[DEBUG_INFO_SIZE] = "";
	char sha1sum[VB2_SHA1_DIGEST_SIZE * 2 + 1];
	char hwid[256];
	uint32_t used = 0;
	VbPublicKey *key;
	VbError_t ret;
	uint32_t i;

	/* Redisplay current screen to overwrite any previous debug output */
	VbDisplayScreen(disp_current_screen, 1, vncptr);

	/* Add hardware ID */
	VbRegionReadHWID(cparams, hwid, sizeof(hwid));
	used += StrnAppend(buf + used, "HWID: ", DEBUG_INFO_SIZE - used);
	used += StrnAppend(buf + used, hwid, DEBUG_INFO_SIZE - used);

	/* Add recovery reason and subcode */
	VbNvGet(vncptr, VBNV_RECOVERY_SUBCODE, &i);
	used += StrnAppend(buf + used,
			"\nrecovery_reason: 0x", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
			       shared->recovery_reason, 16, 2);
	used += StrnAppend(buf + used, " / 0x", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 16, 2);
	used += StrnAppend(buf + used, "  ", DEBUG_INFO_SIZE - used);
	used += StrnAppend(buf + used,
			RecoveryReasonString(shared->recovery_reason),
			DEBUG_INFO_SIZE - used);

	/* Add VbSharedData flags */
	used += StrnAppend(buf + used, "\nVbSD.flags: 0x", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
			       shared->flags, 16, 8);

	/* Add raw contents of VbNvStorage */
	used += StrnAppend(buf + used, "\nVbNv.raw:", DEBUG_INFO_SIZE - used);
	for (i = 0; i < VBNV_BLOCK_SIZE; i++) {
		used += StrnAppend(buf + used, " ", DEBUG_INFO_SIZE - used);
		used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
				       vncptr->raw[i], 16, 2);
	}

	/* Add dev_boot_usb flag */
	VbNvGet(vncptr, VBNV_DEV_BOOT_USB, &i);
	used += StrnAppend(buf + used, "\ndev_boot_usb: ", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 10, 0);

	/* Add dev_boot_legacy flag */
	VbNvGet(vncptr, VBNV_DEV_BOOT_LEGACY, &i);
	used += StrnAppend(buf + used,
			"\ndev_boot_legacy: ", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 10, 0);

	/* Add dev_default_boot flag */
	VbNvGet(vncptr, VBNV_DEV_DEFAULT_BOOT, &i);
	used += StrnAppend(buf + used,
			"\ndev_default_boot: ", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 10, 0);

	/* Add dev_boot_signed_only flag */
	VbNvGet(vncptr, VBNV_DEV_BOOT_SIGNED_ONLY, &i);
	used += StrnAppend(buf + used, "\ndev_boot_signed_only: ",
			DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 10, 0);

	/* Add dev_boot_fastboot_full_cap flag */
	VbNvGet(vncptr, VBNV_DEV_BOOT_FASTBOOT_FULL_CAP, &i);
	used += StrnAppend(buf + used, "\ndev_boot_fastboot_full_cap: ",
			DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used, i, 10, 0);

	/* Add TPM versions */
	used += StrnAppend(buf + used, "\nTPM: fwver=0x", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
			       shared->fw_version_tpm, 16, 8);
	used += StrnAppend(buf + used, " kernver=0x", DEBUG_INFO_SIZE - used);
	used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
			       shared->kernel_version_tpm, 16, 8);

	/* Add GBB flags */
	used += StrnAppend(buf + used, "\ngbb.flags: 0x", DEBUG_INFO_SIZE - used);
	if (gbb->major_version == GBB_MAJOR_VER && gbb->minor_version >= 1) {
		used += Uint64ToString(buf + used, DEBUG_INFO_SIZE - used,
				       gbb->flags, 16, 8);
	} else {
		used += StrnAppend(buf + used,
				"0 (default)", DEBUG_INFO_SIZE - used);
	}

	/* Add sha1sum for Root & Recovery keys */
	ret = VbGbbReadRootKey(cparams, &key);
	if (!ret) {
		FillInSha1Sum(sha1sum, key);
		VbExFree(key);
		used += StrnAppend(buf + used, "\ngbb.rootkey: ",
				   DEBUG_INFO_SIZE - used);
		used += StrnAppend(buf + used, sha1sum,
				   DEBUG_INFO_SIZE - used);
	}

	ret = VbGbbReadRecoveryKey(cparams, &key);
	if (!ret) {
		FillInSha1Sum(sha1sum, key);
		VbExFree(key);
		used += StrnAppend(buf + used, "\ngbb.recovery_key: ",
				   DEBUG_INFO_SIZE - used);
		used += StrnAppend(buf + used, sha1sum,
				   DEBUG_INFO_SIZE - used);
	}

	/* If we're in dev-mode, show the kernel subkey that we expect, too. */
	if (0 == shared->recovery_reason) {
		FillInSha1Sum(sha1sum, &shared->kernel_subkey);
		used += StrnAppend(buf + used,
				"\nkernel_subkey: ", DEBUG_INFO_SIZE - used);
		used += StrnAppend(buf + used, sha1sum, DEBUG_INFO_SIZE - used);
	}

	/* Make sure we finish with a newline */
	used += StrnAppend(buf + used, "\n", DEBUG_INFO_SIZE - used);

	/* TODO: add more interesting data:
	 * - Information on current disks */

	buf[DEBUG_INFO_SIZE - 1] = '\0';
	return VbExDisplayDebugInfo(buf);
}

#define MAGIC_WORD_LEN 5
#define MAGIC_WORD "xyzzy"
static uint8_t MagicBuffer[MAGIC_WORD_LEN];

VbError_t VbCheckDisplayKey(VbCommonParams *cparams, uint32_t key,
                            VbNvContext *vncptr)
{
	int i;

	/* Update key buffer */
	for(i = 1; i < MAGIC_WORD_LEN; i++)
		MagicBuffer[i - 1] = MagicBuffer[i];
	/* Save as lower-case ASCII */
	MagicBuffer[MAGIC_WORD_LEN - 1] = (key | 0x20) & 0xFF;

	if ('\t' == key) {
		/* Tab = display debug info */
		return VbDisplayDebugInfo(cparams, vncptr);
	} else if (VB_KEY_LEFT == key || VB_KEY_RIGHT == key ||
		   VB_KEY_DOWN == key || VB_KEY_UP == key) {
		/* Arrow keys = change localization */
		uint32_t loc = 0;
		uint32_t count = 0;

		VbNvGet(vncptr, VBNV_LOCALIZATION_INDEX, &loc);
		if (VBERROR_SUCCESS != VbGetLocalizationCount(&count))
			loc = 0;  /* No localization count (bad GBB?) */
		else if (VB_KEY_RIGHT == key || VB_KEY_UP == key)
			loc = (loc < count - 1 ? loc + 1 : 0);
		else
			loc = (loc > 0 ? loc - 1 : count - 1);
		VBDEBUG(("VbCheckDisplayKey() - change localization to %d\n",
			 (int)loc));
		VbNvSet(vncptr, VBNV_LOCALIZATION_INDEX, loc);
		VbNvSet(vncptr, VBNV_BACKUP_NVRAM_REQUEST, 1);

#ifdef SAVE_LOCALE_IMMEDIATELY
		VbNvTeardown(vncptr);  /* really only computes checksum */
		if (vncptr->raw_changed)
			VbExNvStorageWrite(vncptr->raw);
#endif

		/* Force redraw of current screen */
		return VbDisplayScreen(disp_current_screen, 1, vncptr);
	}

	if (0 == Memcmp(MagicBuffer, MAGIC_WORD, MAGIC_WORD_LEN)) {
		if (VBEASTEREGG)
			(void)VbDisplayScreen(disp_current_screen, 1, vncptr);
	}

  return VBERROR_SUCCESS;
}
