/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Recovery reason string mapping.
 */

#include "2recovery_reasons.h"
#include "2sysincludes.h"

const char *vb2_get_recovery_reason_string(uint8_t code)
{
	switch (code) {
	case VB2_RECOVERY_NOT_REQUESTED:
		return "Recovery not requested";
	case VB2_RECOVERY_LEGACY:
		return "Recovery requested from legacy utility";
	case VB2_RECOVERY_RO_MANUAL:
		return "Recovery button pressed";
	case VB2_RECOVERY_RO_INVALID_RW:
		return "RW firmware failed signature check";
	case VB2_RECOVERY_RO_SHARED_DATA:
		return "Shared data error in read-only firmware";
	case VB2_RECOVERY_FW_KEYBLOCK:
		return "RW firmware unable to verify keyblock";
	case VB2_RECOVERY_FW_KEY_ROLLBACK:
		return "RW firmware key version rollback detected";
	case VB2_RECOVERY_FW_PREAMBLE:
		return "RW firmware unable to verify preamble";
	case VB2_RECOVERY_FW_ROLLBACK:
		return "RW firmware version rollback detected";
	case VB2_RECOVERY_FW_BODY:
		return "RW firmware unable to verify firmware body";
	case VB2_RECOVERY_RO_FIRMWARE:
		return "Firmware problem outside of verified boot";
	case VB2_RECOVERY_RO_TPM_REBOOT:
		return "TPM requires a system reboot (should be transient)";
	case VB2_RECOVERY_EC_SOFTWARE_SYNC:
		return "EC software sync error";
	case VB2_RECOVERY_EC_UNKNOWN_IMAGE:
		return "EC software sync unable to determine active EC image";
	case VB2_RECOVERY_EC_UPDATE:
		return "EC software sync error updating EC";
	case VB2_RECOVERY_EC_JUMP_RW:
		return "EC software sync unable to jump to EC-RW";
	case VB2_RECOVERY_EC_PROTECT:
		return "EC software sync protection error";
	case VB2_RECOVERY_EC_EXPECTED_HASH:
		return "EC software sync error "
			"obtaining expected EC hash from BIOS";
	case VB2_RECOVERY_SECDATA_FIRMWARE_INIT:
		return "Firmware secure NVRAM (TPM) initialization error";
	case VB2_RECOVERY_GBB_HEADER:
		return "Error parsing GBB header";
	case VB2_RECOVERY_TPM_CLEAR_OWNER:
		return "Error trying to clear TPM owner";
	case VB2_RECOVERY_DEV_SWITCH:
		return "Error reading or updating developer switch";
	case VB2_RECOVERY_FW_SLOT:
		return "Error selecting RW firmware slot";
	case VB2_RECOVERY_AUX_FW_UPDATE:
		return "Error updating AUX firmware";
	case VB2_RECOVERY_RO_UNSPECIFIED:
		return "Unspecified/unknown error in RO firmware";
	case VB2_RECOVERY_RW_INVALID_OS:
		return "OS kernel or rootfs failed signature check";
	case VB2_RECOVERY_RW_SHARED_DATA:
		return "Shared data error in rewritable firmware";
	case VB2_RECOVERY_TPM_E_FAIL:
		return "TPM error that was not fixed by reboot";
	case VB2_RECOVERY_RO_TPM_S_ERROR:
		return "TPM setup error in read-only firmware";
	case VB2_RECOVERY_RO_TPM_W_ERROR:
		return "TPM write error in read-only firmware";
	case VB2_RECOVERY_RO_TPM_L_ERROR:
		return "TPM lock error in read-only firmware";
	case VB2_RECOVERY_RO_TPM_U_ERROR:
		return "TPM update error in read-only firmware";
	case VB2_RECOVERY_RW_TPM_R_ERROR:
		return "TPM read error in rewritable firmware";
	case VB2_RECOVERY_RW_TPM_W_ERROR:
		return "TPM write error in rewritable firmware";
	case VB2_RECOVERY_RW_TPM_L_ERROR:
		return "TPM lock error in rewritable firmware";
	case VB2_RECOVERY_EC_HASH_FAILED:
		return "EC software sync unable to get EC image hash";
	case VB2_RECOVERY_EC_HASH_SIZE:
		return "EC software sync invalid image hash size";
	case VB2_RECOVERY_LK_UNSPECIFIED:
		return "Unspecified error while trying to load kernel";
	case VB2_RECOVERY_RW_NO_DISK:
		return "No bootable storage device in system";
	case VB2_RECOVERY_RW_NO_KERNEL:
		return "No bootable kernel found on disk";
	case VB2_RECOVERY_SECDATA_KERNEL_INIT:
		return "Kernel secure NVRAM (TPM) initialization error";
	case VB2_RECOVERY_RO_TPM_REC_HASH_L_ERROR:
		return "Recovery hash space lock error in RO firmware";
	case VB2_RECOVERY_TPM_DISABLE_FAILED:
		return "Failed to disable TPM before running untrusted code";
	case VB2_RECOVERY_ALTFW_HASH_FAILED:
		return "Verification of alternative firmware payload failed";
	case VB2_RECOVERY_CR50_BOOT_MODE:
		return "Failed to get boot mode from Cr50";
	case VB2_RECOVERY_ESCAPE_NO_BOOT:
		return "Attempt to escape from NO_BOOT mode was detected";
	case VB2_RECOVERY_RW_UNSPECIFIED:
		return "Unspecified/unknown error in RW firmware";
	case VB2_RECOVERY_US_TEST:
		return "Recovery mode test from user-mode";
	case VB2_RECOVERY_TRAIN_AND_REBOOT:
		return "User-mode requested DRAM train and reboot";
	case VB2_RECOVERY_US_UNSPECIFIED:
		return "Unspecified/unknown error in user-mode";
	}
	return "Unknown or deprecated error code";
}
