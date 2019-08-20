/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for rollback_index functions
 */

#include "2crc8.h"
#include "2secdata.h"
#include "2sysincludes.h"
#include "rollback_index.h"
#include "test_common.h"
#include "tlcl.h"
#include "tss_constants.h"

uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
	__attribute__ ((aligned (VB2_WORKBUF_ALIGN)));
struct vb2_context c = {
	.workbuf = workbuf,
	.workbuf_size = sizeof(workbuf),
};

/*
 * Buffer to hold accumulated list of calls to mocked Tlcl functions.
 * Each function appends itself to the buffer and updates mock_cnext.
 *
 * Size of mock_calls[] should be big enough to handle all expected
 * call sequences; 16KB should be plenty since none of the sequences
 * below is more than a few hundred bytes.  We could be more clever
 * and use snprintf() with length checking below, at the expense of
 * making all the mock implementations bigger.  If this were code used
 * outside of unit tests we'd want to do that, but here if we did
 * overrun the buffer the worst that's likely to happen is we'll crash
 * the test, and crash = failure anyway.
 */
static char mock_calls[16384];
static char *mock_cnext = mock_calls;

/*
 * Variables to support mocked error values from Tlcl functions.  Each
 * call, mock_count is incremented.  If mock_count==fail_at_count, return
 * fail_with_error instead of the normal return value.
 */
static int mock_count = 0;
static int fail_at_count = 0;
static uint32_t fail_with_error = TPM_SUCCESS;

/* Params / backing store for mocked Tlcl functions. */
static TPM_PERMANENT_FLAGS mock_pflags;
static RollbackSpaceFirmware mock_rsf;
static RollbackSpaceKernel mock_rsk;

static union {
	struct RollbackSpaceFwmp fwmp;
	uint8_t buf[VB2_SECDATA_FWMP_MAX_SIZE];
} mock_fwmp;
static uint32_t mock_fwmp_real_size;

static uint32_t mock_permissions;

/* Reset the variables for the Tlcl mock functions. */
static void ResetMocks(int fail_on_call, uint32_t fail_with_err)
{
	*mock_calls = 0;
	mock_cnext = mock_calls;
	mock_count = 0;
	fail_at_count = fail_on_call;
	fail_with_error = fail_with_err;

	memset(&mock_pflags, 0, sizeof(mock_pflags));

	memset(&mock_rsf, 0, sizeof(mock_rsf));
	memset(&mock_rsk, 0, sizeof(mock_rsk));

	mock_permissions = TPM_NV_PER_PPWRITE;

	memset(mock_fwmp.buf, 0, sizeof(mock_fwmp.buf));
	mock_fwmp.fwmp.struct_size = sizeof(mock_fwmp.fwmp);
	mock_fwmp.fwmp.struct_version = VB2_SECDATA_FWMP_VERSION;
	mock_fwmp.fwmp.flags = 0x1234;
	/* Put some data in the hash */
	mock_fwmp.fwmp.dev_key_hash[0] = 0xaa;
	mock_fwmp.fwmp.dev_key_hash[VB2_SECDATA_FWMP_HASH_SIZE - 1] = 0xbb;
	mock_fwmp_real_size = sizeof(mock_fwmp.fwmp);
}

/* Mock functions */

vb2_error_t vb2api_secdata_fwmp_check(struct vb2_context *ctx, uint32_t *size)
{
	if (*size < mock_fwmp_real_size) {
		*size = mock_fwmp_real_size;
		return VB2_ERROR_SECDATA_FWMP_INCOMPLETE;
	}
	return VB2_SUCCESS;
}

/****************************************************************************/
/* Mocks for tlcl functions which log the calls made to mock_calls[]. */

uint32_t TlclLibInit(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclLibInit()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclStartup(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclStartup()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclResume(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclResume()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclForceClear(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclForceClear()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclSetEnable(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclSetEnable()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclSetDeactivated(uint8_t flag)
{
	mock_cnext += sprintf(mock_cnext, "TlclSetDeactivated(%d)\n", flag);
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclRead(uint32_t index, void* data, uint32_t length)
{
	mock_cnext += sprintf(mock_cnext, "TlclRead(0x%x, %d)\n",
			      index, length);

	if (FIRMWARE_NV_INDEX == index) {
		TEST_EQ(length, sizeof(mock_rsf), "TlclRead rsf size");
		memcpy(data, &mock_rsf, length);
	} else if (KERNEL_NV_INDEX == index) {
		TEST_EQ(length, sizeof(mock_rsk), "TlclRead rsk size");
		memcpy(data, &mock_rsk, length);
	} else if (FWMP_NV_INDEX == index) {
		memset(data, 0, length);
		if (length > sizeof(mock_fwmp))
			length = sizeof(mock_fwmp);
		memcpy(data, &mock_fwmp, length);
	} else {
		memset(data, 0, length);
	}

	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length)
{
	mock_cnext += sprintf(mock_cnext, "TlclWrite(0x%x, %d)\n",
			      index, length);

	if (FIRMWARE_NV_INDEX == index) {
		TEST_EQ(length, sizeof(mock_rsf), "TlclWrite rsf size");
		memcpy(&mock_rsf, data, length);
	} else if (KERNEL_NV_INDEX == index) {
		TEST_EQ(length, sizeof(mock_rsk), "TlclWrite rsk size");
		memcpy(&mock_rsk, data, length);
	}

	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size)
{
	mock_cnext += sprintf(mock_cnext, "TlclDefineSpace(0x%x, 0x%x, %d)\n",
			      index, perm, size);
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclSelfTestFull(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclSelfTestFull()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclContinueSelfTest(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclContinueSelfTest()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclGetPermanentFlags(TPM_PERMANENT_FLAGS *pflags)
{
	mock_cnext += sprintf(mock_cnext, "TlclGetPermanentFlags()\n");
	memcpy(pflags, &mock_pflags, sizeof(mock_pflags));
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

/* TlclGetFlags() doesn't need mocking; it calls TlclGetPermanentFlags() */

uint32_t TlclAssertPhysicalPresence(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclAssertPhysicalPresence()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclFinalizePhysicalPresence(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclFinalizePhysicalPresence()\n");
	mock_pflags.physicalPresenceLifetimeLock = 1;
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclPhysicalPresenceCMDEnable(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclPhysicalPresenceCMDEnable()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclSetNvLocked(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclSetNvLocked()\n");
	mock_pflags.nvLocked = 1;
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclSetGlobalLock(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclSetGlobalLock()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclLockPhysicalPresence(void)
{
	mock_cnext += sprintf(mock_cnext, "TlclLockPhysicalPresence()\n");
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

uint32_t TlclGetPermissions(uint32_t index, uint32_t* permissions)
{
	mock_cnext += sprintf(mock_cnext, "TlclGetPermissions(0x%x)\n", index);
	*permissions = mock_permissions;
	return (++mock_count == fail_at_count) ? fail_with_error : TPM_SUCCESS;
}

/****************************************************************************/
/* Tests for CRC errors  */

static void FirmwareSpaceTest(void)
{
	RollbackSpaceFirmware rsf;

	/* Not present is an error */
	ResetMocks(1, TPM_E_BADINDEX);
	TEST_EQ(ReadSpaceFirmware(&rsf), TPM_E_BADINDEX,
		"ReadSpaceFirmware(), not present");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x1007, 10)\n",
		    "  tlcl calls");

	/* Read failure */
	ResetMocks(1, TPM_E_IOERROR);
	TEST_EQ(ReadSpaceFirmware(&rsf), TPM_E_IOERROR,
		"ReadSpaceFirmware(), failure");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x1007, 10)\n",
		    "  tlcl calls");

	/* Read success */
	ResetMocks(0, 0);
	TEST_EQ(ReadSpaceFirmware(&rsf), TPM_SUCCESS,
		"ReadSpaceFirmware(), success");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x1007, 10)\n",
		    "  tlcl calls");
	TEST_EQ(memcmp(&rsf, &mock_rsf, sizeof(rsf)), 0, "  data");

	/* Write failure */
	ResetMocks(1, TPM_E_IOERROR);
	TEST_EQ(WriteSpaceFirmware(&rsf), TPM_E_IOERROR,
		"WriteSpaceFirmware(), failure");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x1007, 10)\n",
		    "  tlcl calls");

	/* Write success and readback */
	ResetMocks(0, 0);
	memset(&rsf, 0xa6, sizeof(rsf));
	TEST_EQ(WriteSpaceFirmware(&rsf), TPM_SUCCESS,
		"WriteSpaceFirmware(), success");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x1007, 10)\n",
		    "  tlcl calls");
	memset(&rsf, 0xa6, sizeof(rsf));
	TEST_EQ(memcmp(&rsf, &mock_rsf, sizeof(rsf)), 0,
		"  unchanged on readback");
}

static void KernelSpaceTest(void)
{
	RollbackSpaceKernel rsk;

	/* Not present is an error */
	ResetMocks(1, TPM_E_BADINDEX);
	TEST_EQ(ReadSpaceKernel(&rsk), TPM_E_BADINDEX,
		"ReadSpaceKernel(), not present");
	TEST_STR_EQ(mock_calls,
		    "TlclGetPermissions(0x1008)\n",
		    "  tlcl calls");

	/* Bad permissions */
	ResetMocks(0, 0);
	mock_permissions = 0;
	TEST_EQ(ReadSpaceKernel(&rsk), TPM_E_CORRUPTED_STATE,
		"ReadSpaceKernel(), bad permissions");
	TEST_STR_EQ(mock_calls,
		    "TlclGetPermissions(0x1008)\n",
		    "  tlcl calls");

	/* Good permissions, read failure */
	ResetMocks(2, TPM_E_IOERROR);
	TEST_EQ(ReadSpaceKernel(&rsk), TPM_E_IOERROR,
		"ReadSpaceKernel(), good permissions, failure");
	TEST_STR_EQ(mock_calls,
		    "TlclGetPermissions(0x1008)\n"
		    "TlclRead(0x1008, 13)\n",
		    "  tlcl calls");

	/* Good permissions, read success */
	ResetMocks(0, 0);
	TEST_EQ(ReadSpaceKernel(&rsk), TPM_SUCCESS,
		"ReadSpaceKernel(), good permissions, success");
	TEST_STR_EQ(mock_calls,
		    "TlclGetPermissions(0x1008)\n"
		    "TlclRead(0x1008, 13)\n",
		    "  tlcl calls");
	TEST_EQ(memcmp(&rsk, &mock_rsk, sizeof(rsk)), 0, "  data");

	/* Write failure */
	ResetMocks(1, TPM_E_IOERROR);
	TEST_EQ(WriteSpaceKernel(&rsk), TPM_E_IOERROR,
		"WriteSpaceKernel(), failure");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x1008, 13)\n",
		    "  tlcl calls");

	/* Write success and readback */
	ResetMocks(0, 0);
	memset(&rsk, 0xa6, sizeof(rsk));
	TEST_EQ(WriteSpaceKernel(&rsk), TPM_SUCCESS,
		"WriteSpaceKernel(), failure");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x1008, 13)\n",
		    "  tlcl calls");
	memset(&rsk, 0xa6, sizeof(rsk));
	TEST_EQ(memcmp(&rsk, &mock_rsk, sizeof(rsk)), 0,
		"  unchanged on readback");
}

/****************************************************************************/
/* Tests for RollbackFwmpRead() calls */

static void RollbackFwmpTest(void)
{
	struct RollbackSpaceFwmp *fwmp =
		(struct RollbackSpaceFwmp *)&c.secdata_fwmp;
	struct RollbackSpaceFwmp fwmp_zero = {0};

	/* Read failure */
	ResetMocks(1, TPM_E_IOERROR);
	TEST_EQ(RollbackFwmpRead(&c), TPM_E_IOERROR,
		"RollbackFwmpRead(), failure");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n",
		    "  tlcl calls");

	/* Normal read */
	ResetMocks(0, 0);
	TEST_EQ(RollbackFwmpRead(&c), 0,
		"RollbackFwmpRead(), success");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n",
		    "  tlcl calls");
	TEST_EQ(memcmp(fwmp, &mock_fwmp, sizeof(*fwmp)), 0, "  data");

	/* Read error */
	ResetMocks(1, TPM_E_IOERROR);
	TEST_EQ(RollbackFwmpRead(&c), TPM_E_IOERROR,
		"RollbackFwmpRead(), error");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n",
		    "  tlcl calls");

	/* Not present isn't an error; just returns empty data */
	ResetMocks(1, TPM_E_BADINDEX);
	TEST_EQ(RollbackFwmpRead(&c), 0, "RollbackFwmpRead(), not present");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n",
		    "  tlcl calls");
	TEST_EQ(memcmp(fwmp, &fwmp_zero, sizeof(*fwmp)), 0, "  data clear");

	/* Struct size too large */
	ResetMocks(0, 0);
	mock_fwmp_real_size += 4;
	TEST_EQ(RollbackFwmpRead(&c), 0, "RollbackFwmpRead(), bigger");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n"
		    "TlclRead(0x100a, 44)\n",
		    "  tlcl calls");
	TEST_EQ(memcmp(fwmp, &mock_fwmp, mock_fwmp_real_size), 0, "  data");
}

/****************************************************************************/
/* Tests for misc helper functions */

static void MiscTest(void)
{
	uint8_t buf[8];

	ResetMocks(0, 0);
	TEST_EQ(TPMClearAndReenable(), 0, "TPMClearAndReenable()");
	TEST_STR_EQ(mock_calls,
		    "TlclForceClear()\n"
		    "TlclSetEnable()\n"
		    "TlclSetDeactivated(0)\n",
		    "  tlcl calls");

	ResetMocks(0, 0);
	TEST_EQ(SafeWrite(0x123, buf, 8), 0, "SafeWrite()");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x123, 8)\n",
		    "  tlcl calls");

	ResetMocks(1, TPM_E_BADINDEX);
	TEST_EQ(SafeWrite(0x123, buf, 8), TPM_E_BADINDEX, "SafeWrite() bad");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x123, 8)\n",
		    "  tlcl calls");

	ResetMocks(1, TPM_E_MAXNVWRITES);
	TEST_EQ(SafeWrite(0x123, buf, 8), 0, "SafeWrite() retry max writes");
	TEST_STR_EQ(mock_calls,
		    "TlclWrite(0x123, 8)\n"
		    "TlclForceClear()\n"
		    "TlclSetEnable()\n"
		    "TlclSetDeactivated(0)\n"
		    "TlclWrite(0x123, 8)\n",
		    "  tlcl calls");
}

int main(int argc, char* argv[])
{
	FirmwareSpaceTest();
	KernelSpaceTest();
	RollbackFwmpTest();
	MiscTest();

	return gTestSuccess ? 0 : 255;
}
