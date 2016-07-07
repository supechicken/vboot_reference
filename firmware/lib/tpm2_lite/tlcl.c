/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some TPM constants and type definitions for standalone compilation for use
 * in the firmware
 */

#include "tpm2_marshaling.h"
#include "utility.h"

#define TPM2_NOT_IMPLEMENTED TPM_E_BADTAG

static void *tpm_process_command(TPM_CC command, void *command_body)
{
	/* Command/response buffer. */
	static uint8_t cr_buffer[TPM_BUFFER_SIZE];
	uint32_t out_size, in_size;

	out_size = tpm_marshal_command(command, command_body,
				       cr_buffer, sizeof(cr_buffer));
	if (out_size < 0) {
		VBDEBUG(("command %#x, cr size %d\n",
			 command, out_size));
		return NULL;
	}

	in_size = sizeof(cr_buffer);
	if (VbExTpmSendReceive(cr_buffer, out_size,
			       cr_buffer, &in_size) != TPM_SUCCESS) {
		VBDEBUG(("tpm transaction failed\n"));
		return NULL;
	}

	return tpm_unmarshal_response(command, cr_buffer, in_size);
}

/**
 *  * Call this first.  Returns 0 if success, nonzero if error.
 *   */
uint32_t TlclLibInit(void)
{
	return 0;
}

/**
 *  * Call this on shutdown.  Returns 0 if success, nonzero if error.
 *   */
uint32_t TlclLibClose(void)
{
	return 0;
}

/**
 * Perform a raw TPM request/response transaction.
 */
uint32_t TlclSendReceive(const uint8_t *request, uint8_t *response,
                         int max_length)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Return the size of a TPM request or response packet.
 */
int TlclPacketSize(const uint8_t *packet)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return 0;
}

/**
 * Send a TPM_Startup(ST_CLEAR).  The TPM error code is returned (0 for
 * success).
 */
uint32_t TlclStartup(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Save the TPM state.  Normally done by the kernel before a suspend, included
 * here for tests.  The TPM error code is returned (0 for success).
 */
uint32_t TlclSaveState(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Resume by sending a TPM_Startup(ST_STATE).  The TPM error code is returned
 * (0 for success).
 */
uint32_t TlclResume(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Run the self test.
 *
 * Note---this is synchronous.  To run this in parallel with other firmware,
 * use ContinueSelfTest().  The TPM error code is returned.
 */
uint32_t TlclSelfTestFull(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Run the self test in the background.
 */
uint32_t TlclContinueSelfTest(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Define a space with permission [perm].  [index] is the index for the space,
 * [size] the usable data size.  The TPM error code is returned.
 */
uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Read [length] bytes from space at [index] into [data].  The TPM error code
 * is returned.
 */
uint32_t TlclRead(uint32_t index, void* data, uint32_t length)
{
	struct tpm2_nv_read_cmd nv_readc;
	struct tpm2_response *response;

	Memset(&nv_readc, 0, sizeof(nv_readc));

	nv_readc.nvIndex = HR_NV_INDEX + index;
	nv_readc.size = length;

	response = tpm_process_command(TPM2_NV_Read, &nv_readc);

	/* Need to map tpm error codes into internal values. */
	if (!response)
		return TPM_E_READ_FAILURE;

	VBDEBUG(("%s:%d index %#x return code %x\n",
		 __FILE__, __LINE__, index, response->hdr.tpm_code));
	switch (response->hdr.tpm_code) {
	case 0:
		break;

	case 0x28b:
		return TPM_E_BADINDEX;

	default:
		return TPM_E_READ_FAILURE;
	}

	if (length > response->nvr.buffer.t.size)
		return TPM_E_RESPONSE_TOO_LARGE;

	if (length < response->nvr.buffer.t.size)
		return TPM_E_READ_EMPTY;

	Memcpy(data, response->nvr.buffer.t.buffer, length);

	return TPM_SUCCESS;
}

/**
 * Write [length] bytes of [data] to space at [index].  The TPM error code is
 * returned.
 */
uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length)
{
	struct tpm2_nv_write_cmd nv_writec;
	struct tpm2_response *response;

	Memset(&nv_writec, 0, sizeof(nv_writec));

	nv_writec.nvIndex = HR_NV_INDEX + index;
	nv_writec.data.t.size = length;
	nv_writec.data.t.buffer = data;

	response = tpm_process_command(TPM2_NV_Write, &nv_writec);

	/* Need to map tpm error codes into internal values. */
	if (!response)
		return TPM_E_WRITE_FAILURE;

	VBDEBUG(("%s:%d return code %x\n", __func__, __LINE__,
		 response->hdr.tpm_code));

	return TPM_SUCCESS;
}

/**
 * Read PCR at [index] into [data].  [length] must be TPM_PCR_DIGEST or
 * larger. The TPM error code is returned.
 */
uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Write-lock space at [index].  The TPM error code is returned.
 */
uint32_t TlclWriteLock(uint32_t index)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Read-lock space at [index].  The TPM error code is returned.
 */
uint32_t TlclReadLock(uint32_t index)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Assert physical presence in software.  The TPM error code is returned.
 */
uint32_t TlclAssertPhysicalPresence(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Enable the physical presence command.  The TPM error code is returned.
 */
uint32_t TlclPhysicalPresenceCMDEnable(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Finalize the physical presence settings: sofware PP is enabled, hardware PP
 * is disabled, and the lifetime lock is set.  The TPM error code is returned.
 */
uint32_t TlclFinalizePhysicalPresence(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

uint32_t TlclAssertPhysicalPresenceResult(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Turn off physical presence and locks it off until next reboot.  The TPM
 * error code is returned.
 */
uint32_t TlclLockPhysicalPresence(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM_SUCCESS;
}

/**
 * Set the nvLocked bit.  The TPM error code is returned.
 */
uint32_t TlclSetNvLocked(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Return 1 if the TPM is owned, 0 otherwise.
 */
int TlclIsOwned(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return 0;
}

/**
 * Issue a ForceClear.  The TPM error code is returned.
 */
uint32_t TlclForceClear(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM_SUCCESS;
}

/**
 * Issue a PhysicalEnable.  The TPM error code is returned.
 */
uint32_t TlclSetEnable(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM_SUCCESS;
}

/**
 * Issue a PhysicalDisable.  The TPM error code is returned.
 */
uint32_t TlclClearEnable(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Issue a SetDeactivated.  Pass 0 to activate.  Returns result code.
 */
uint32_t TlclSetDeactivated(uint8_t flag)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM_SUCCESS;
}

/**
 * Get flags of interest.  Pointers for flags you aren't interested in may
 * be NULL.  The TPM error code is returned.
 */
uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
                      uint8_t *nvlocked)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Set the bGlobalLock flag, which only a reboot can clear.  The TPM error
 * code is returned.
 */
uint32_t TlclSetGlobalLock(void)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Perform a TPM_Extend.
 */
uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest, uint8_t *out_digest)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Get the permission bits for the NVRAM space with |index|.
 */
uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions)
{
	*permissions = 0;
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM_SUCCESS;
}

/**
 * Get the entire set of permanent flags.
 */
uint32_t TlclGetPermanentFlags(TPM_PERMANENT_FLAGS *pflags)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Get the entire set of volatile (ST_CLEAR) flags.
 */
uint32_t TlclGetSTClearFlags(TPM_STCLEAR_FLAGS *pflags)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Get the ownership flag. The TPM error code is returned.
 */
uint32_t TlclGetOwnership(uint8_t *owned)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

/**
 * Request [length] bytes from TPM RNG to be stored in [data]. Actual number of
 * bytes read is stored in [size]. The TPM error code is returned.
 */
uint32_t TlclGetRandom(uint8_t *data, uint32_t length, uint32_t *size)
{
	VBDEBUG(("%s called, NOT YET IMPLEMENTED\n", __func__));
	return TPM2_NOT_IMPLEMENTED;
}

