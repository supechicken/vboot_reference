/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common part of TPM error description code.
 *
 */

#include "tpm_error_messages.h"
#include "tss_constants.h"

#define TPM_E_BASE 0x800
#define TPM_E_NON_FATAL 0x800

typedef struct tpm_error_entry {
  uint32_t error_code;
  tpm_error_info info;
} tpm_error_entry;

/* TPM 1.2 error codes.
 *
 * Copy-pasted and lightly edited from TCG TPM Main Part 2 TPM Structures
 * Version 1.2 Level 2 Revision 103 26 October 2006 Draft.
 */

#define QNT_ERROR_TPM1 (sizeof(tpm_error_table_tpm1)/sizeof(tpm_error_info))
tpm_error_info tpm_error_table_tpm1[] = {
/* 0 */
{ "TPM_SUCCESS",
  "Operation was successful" },
{ "TPM_AUTHFAIL",
  "Authentication failed" },
{ "TPM_BADINDEX",
  "The index to a PCR, DIR or other register is incorrect" },
{ "TPM_BAD_PARAMETER",
  "One or more parameter is bad" },
{ "TPM_AUDITFAILURE",
  "An operation completed successfully\n"
  "but the auditing of that operation failed" },
{ "TPM_CLEAR_DISABLED",
  "The clear disable flag is set and all clear operations now require\n"
  "physical access" },
{ "TPM_DEACTIVATED",
  "The TPM is deactivated" },
{ "TPM_DISABLED",
  "The TPM is disabled" },
{ "TPM_DISABLED_CMD",
  "The target command has been disabled" },
{ "TPM_FAIL",
  "The operation failed" },
/* 10 */
{ "TPM_BAD_ORDINAL",
  "The ordinal was unknown or inconsistent" },
{ "TPM_INSTALL_DISABLED",
  "The ability to install an owner is disabled" },
{ "TPM_INVALID_KEYHANDLE",
  "The key handle can not be interpreted" },
{ "TPM_KEYNOTFOUND",
  "The key handle points to an invalid key" },
{ "TPM_INAPPROPRIATE_ENC",
  "Unacceptable encryption scheme" },
{ "TPM_MIGRATEFAIL",
  "Migration authorization failed" },
{ "TPM_INVALID_PCR_INFO",
  "PCR information could not be interpreted" },
{ "TPM_NOSPACE",
  "No room to load key" },
{ "TPM_NOSRK",
  "There is no SRK set" },
{ "TPM_NOTSEALED_BLOB",
  "An encrypted blob is invalid or was not created by this TPM" },
/* 20 */
{ "TPM_OWNER_SET",
  "There is already an Owner" },
{ "TPM_RESOURCES",
  "The TPM has insufficient internal resources to perform the "
  "requested action" },
{ "TPM_SHORTRANDOM",
  "A random string was too short" },
{ "TPM_SIZE",
  "The TPM does not have the space to perform the operation" },
{ "TPM_WRONGPCRVAL",
  "The named PCR value does not match the current PCR value" },
{ "TPM_BAD_PARAM_SIZE",
  "The paramSize argument to the command has the incorrect value" },
{ "TPM_SHA_THREAD",
  "There is no existing SHA-1 thread" },
{ "TPM_SHA_ERROR",
  "The calculation is unable to proceed because the existing SHA-1\n"
  "thread has already encountered an error" },
{ "TPM_FAILEDSELFTEST",
  "Self-test has failed and the TPM has shutdown" },
{ "TPM_AUTH2FAIL",
  "The authorization for the second key in a 2 key function\n"
  "failed authorization" },
/* 30 */
{ "TPM_BADTAG",
  "The tag value sent to for a command is invalid" },
{ "TPM_IOERROR",
  "An IO error occurred transmitting information to the TPM" },
{ "TPM_ENCRYPT_ERROR",
  "The encryption process had a problem" },
{ "TPM_DECRYPT_ERROR",
  "The decryption process did not complete" },
{ "TPM_INVALID_AUTHHANDLE",
  "An invalid handle was used" },
{ "TPM_NO_ENDORSEMENT",
  "The TPM does not a EK installed" },
{ "TPM_INVALID_KEYUSAGE",
  "The usage of a key is not allowed" },
{ "TPM_WRONG_ENTITYTYPE",
  "The submitted entity type is not allowed" },
{ "TPM_INVALID_POSTINIT",
  "The command was received in the wrong sequence relative to TPM_Init\n"
  "and a subsequent TPM_Startup" },
{ "TPM_INAPPROPRIATE_SIG",
  "Signed data cannot include additional DER information" },
/* 40 */
{ "TPM_BAD_KEY_PROPERTY",
  "The key properties in TPM_KEY_PARMs are not supported by this TPM" },
{ "TPM_BAD_MIGRATION",
  "The migration properties of this key are incorrect" },
{ "TPM_BAD_SCHEME",
  "The signature or encryption scheme for this key is incorrect or not\n"
  "permitted in this situation" },
{ "TPM_BAD_DATASIZE",
  "The size of the data (or blob) parameter is bad or inconsistent\n"
  "with the referenced key" },
{ "TPM_BAD_MODE",
  "A mode parameter is bad, such as capArea or subCapArea for\n"
  "TPM_GetCapability, physicalPresence parameter for TPM_PhysicalPresence,\n"
  "or migrationType for, TPM_CreateMigrationBlob" },
{ "TPM_BAD_PRESENCE",
  "Either the physicalPresence or physicalPresenceLock bits\n"
  "have the wrong value" },
{ "TPM_BAD_VERSION",
  "The TPM cannot perform this version of the capability" },
{ "TPM_NO_WRAP_TRANSPORT",
  "The TPM does not allow for wrapped transport sessions" },
{ "TPM_AUDITFAIL_UNSUCCESSFUL",
  "TPM audit construction failed and the underlying command\n"
  "was returning a failure code also" },
{ "TPM_AUDITFAIL_SUCCESSFUL",
  "TPM audit construction failed and the underlying command\n"
  "was returning success" },
/* 50 */
{ "TPM_NOTRESETABLE",
  "Attempt to reset a PCR register that does not have the resettable "
  "attribute" },
{ "TPM_NOTLOCAL",
  "Attempt to reset a PCR register that requires locality\n"
  "and locality modifier not part of command transport" },
{ "TPM_BAD_TYPE",
  "Make identity blob not properly typed" },
{ "TPM_INVALID_RESOURCE",
  "When saving context identified resource type does not match actual "
  "resource" },
{ "TPM_NOTFIPS",
  "The TPM is attempting to execute a command only available when in "
  "FIPS mode" },
{ "TPM_INVALID_FAMILY",
  "The command is attempting to use an invalid family ID" },
{ "TPM_NO_NV_PERMISSION",
  "The permission to manipulate the NV storage is not available" },
{ "TPM_REQUIRES_SIGN",
  "The operation requires a signed command" },
{ "TPM_KEY_NOTSUPPORTED",
  "Wrong operation to load an NV key" },
{ "TPM_AUTH_CONFLICT",
  "NV_LoadKey blob requires both owner and blob authorization" },
/* 60 */
{ "TPM_AREA_LOCKED",
  "The NV area is locked and not writable" },
{ "TPM_BAD_LOCALITY",
  "The locality is incorrect for the attempted operation" },
{ "TPM_READ_ONLY",
  "The NV area is read only and canât be written to" },
{ "TPM_PER_NOWRITE",
  "There is no protection on the write to the NV area" },
{ "TPM_FAMILYCOUNT",
  "The family count value does not match" },
{ "TPM_WRITE_LOCKED",
  "The NV area has already been written to" },
{ "TPM_BAD_ATTRIBUTES",
  "The NV area attributes conflict" },
{ "TPM_INVALID_STRUCTURE",
  "The structure tag and version are invalid or inconsistent" },
{ "TPM_KEY_OWNER_CONTROL",
  "The key is under control of the TPM Owner and can only be evicted\n"
  "by the TPM Owner" },
{ "TPM_BAD_COUNTER",
  "The counter handle is incorrect" },
/* 70 */
{ "TPM_NOT_FULLWRITE",
  "The write is not a complete write of the area" },
{ "TPM_CONTEXT_GAP",
  "The gap between saved context counts is too large" },
{ "TPM_MAXNVWRITES",
  "The maximum number of NV writes without an owner has been exceeded" },
{ "TPM_NOOPERATOR",
  "No operator AuthData value is set" },
{ "TPM_RESOURCEMISSING",
  "The resource pointed to by context is not loaded" },
{ "TPM_DELEGATE_LOCK",
  "The delegate administration is locked" },
{ "TPM_DELEGATE_FAMILY",
  "Attempt to manage a family other then the delegated family" },
{ "TPM_DELEGATE_ADMIN",
  "Delegation table management not enabled" },
{ "TPM_TRANSPORT_NOTEXCLUSIVE",
  "There was a command executed outside of an exclusive transport session" },
{ "TPM_OWNER_CONTROL",
  "Attempt to context save a owner evict controlled key" },
/* 80 */
{ "TPM_DAA_RESOURCES",
  "The DAA command has no resources available to execute the command" },
{ "TPM_DAA_INPUT_DATA0",
  "The consistency check on DAA parameter inputData0 has failed" },
{ "TPM_DAA_INPUT_DATA1",
  "The consistency check on DAA parameter inputData1 has failed" },
{ "TPM_DAA_ISSUER_SETTINGS",
  "The consistency check on DAA_issuerSettings has failed" },
{ "TPM_DAA_TPM_SETTINGS",
  "The consistency check on DAA_tpmSpecific has failed" },
{ "TPM_DAA_STAGE",
  "The atomic process indicated by the submitted DAA command is not\n"
  "the expected process" },
{ "TPM_DAA_ISSUER_VALIDITY",
  "The issuerâs validity check has detected an inconsistency" },
{ "TPM_DAA_WRONG_W",
  "The consistency check on w has failed" },
{ "TPM_BAD_HANDLE",
  "The handle is incorrect" },
{ "TPM_BAD_DELEGATE",
  "Delegation is not correct" },
/* 90 */
{ "TPM_BADCONTEXT",
  "The context blob is invalid" },
{ "TPM_TOOMANYCONTEXTS",
  "Too many contexts held by the TPM" },
{ "TPM_MA_TICKET_SIGNATURE",
  "Migration authority signature validation failure" },
{ "TPM_MA_DESTINATION",
  "Migration destination not authenticated" },
{ "TPM_MA_SOURCE",
  "Migration source incorrect" },
{ "TPM_MA_AUTHORITY",
  "Incorrect migration authority" },
{ NULL, /* no error info for TPM_E_BASE + 96 */
  NULL },
{ "TPM_PERMANENTEK",
  "Attempt to revoke the EK and the EK is not revocable" },
{ "TPM_BAD_SIGNATURE",
  "Bad signature of CMK ticket" },
{ "TPM_NOCONTEXTSPACE",
  "There is no room in the context list for additional contexts" },
};

#define QNT_WARNING_TPM1 (sizeof(tpm_warning_table_tpm1)/sizeof(tpm_error_info))
/* Warnings (non fatal errors) start at TPM_E_BASE + TPM_E_NON_FATAL */
tpm_error_info tpm_warning_table_tpm1[] = {
{ "TPM_RETRY",
  "The TPM is too busy to respond to the command immediately, but\n"
  "the command could be resubmitted at a later time.  The TPM MAY\n"
  "return TPM_RETRY for any command at any time" },
{ "TPM_NEEDS_SELFTEST",
  "TPM_ContinueSelfTest has not been run" },
{ "TPM_DOING_SELFTEST",
  "The TPM is currently executing the actions of TPM_ContinueSelfTest\n"
  "because the ordinal required resources that have not been tested" },
{ "TPM_DEFEND_LOCK_RUNNING",
  "The TPM is defending against dictionary attacks and is in some\n"
  "time-out period" },
};

/*
 * Get error info for local errors that can be returned by 1.2 or 2.0 chips.
 */
static tpm_error_info* get_tpm_error_info_local(uint32_t error_code)
{
  return NULL;
}

/*
 * Find error info in the provided tpm_error_info table.
 *
 * @table: table to look in.
 * @first: code of the first entry in the table,
 * @qnt: number of entries in the table.
 * @error_code: error code to look for.
 *
 *  Returns error info if found, or NULL if not.
 */
static tpm_error_info* find_tpm_error_in_table(tpm_error_info* table,
                                               uint32_t first, uint32_t qnt,
                                               uint32_t error_code)
{
  tpm_error_info* info;

  if (error_code < first || error_code >= first + qnt)
    return NULL;
  info = table + (error_code - first);
  if (info->name)
    return info;
  else
    return NULL;
}

/*
 * Get error info for TCG TPM 1.2 errors. Some of them are used to indicate
 * special situations and are mapped to by TPM 2.0 Tlcl, so they are also
 * always checked for both TPM 1.2 and 2.0.
 */
static tpm_error_info* get_tpm_error_info_tpm1(uint32_t error_code)
{
  tpm_error_info* info;

  info = find_tpm_error_in_table(tpm_error_table_tpm1,
                                 TPM_E_BASE,
                                 QNT_ERROR_TPM1,
                                 error_code);
  if (info)
    return info;

  return find_tpm_error_in_table(tpm_warning_table_tpm1,
                                 TPM_E_BASE + TPM_E_NON_FATAL,
                                 QNT_WARNING_TPM1,
                                 error_code);
}

/*
 * Get error info for TPM version (1.2 vs 2.0) specific errors, which are
 * not covered by get_tpm_error_info_comm() or get_tpm_error_info_tpm1().
 * Defined in _tpm1.c and _tpm2.c respectively.
 */
tpm_error_info* get_tpm_error_info_specific(uint32_t error_code);

tpm_error_info* get_tpm_error_info(uint32_t error_code)
{
  tpm_error_info* info;

  info = get_tpm_error_info_local(error_code);
  if (info)
    return info;
  info = get_tpm_error_info_tpm1(error_code);
  if (info)
    return info;
  return get_tpm_error_info_specific(error_code);
}

