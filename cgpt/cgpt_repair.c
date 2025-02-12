/* Copyright 2012 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "cgpt.h"
#include "cgptlib_internal.h"
#include "vboot_host.h"

int CgptRepair(CgptRepairParams *params) {
  struct drive drive;

  if (params == NULL)
    return CGPT_FAILED;

  if (CGPT_OK != DriveOpen(params->drive_name, &drive, O_RDWR,
                           params->drive_size))
    return CGPT_FAILED;

  int gpt_retval = GptValidityCheck(&drive.gpt);
  if (params->verbose)
    printf("GptValidityCheck() returned %d: %s\n",
           gpt_retval, GptError(gpt_retval));

  GptRepair(&drive.gpt);
  if (drive.gpt.modified & GPT_MODIFIED_HEADER1)
    printf("Primary Header is updated.\n");
  if (drive.gpt.modified & GPT_MODIFIED_ENTRIES1)
    printf("Primary Entries is updated.\n");
  if (drive.gpt.modified & GPT_MODIFIED_ENTRIES2)
    printf("Secondary Entries is updated.\n");
  if (drive.gpt.modified & GPT_MODIFIED_HEADER2)
    printf("Secondary Header is updated.\n");

	/*
	 * If the drive size increased (say, volume expansion),
	 * the secondary header/entries moved to end of drive,
	 * but both headers do not reflect the new drive size
	 * (Alternate LBA in primary; Last Usable LBA in both).
	 *
	 * Per the UEFI spec, first move the secondary header
	 * to the end of drive (done above), and later update
	 * primary/secondary headers to reflect the new size.
	 *
	 * Note: do not check for last_usable_lba, as it does
	 * not change if '-D' is specified (run_cgpt_tests.sh).
	 */
	GptHeader *header1 = (GptHeader *)(drive.gpt.primary_header);
	GptHeader *header2 = (GptHeader *)(drive.gpt.secondary_header);
	if ((header1->alternate_lba < header2->my_lba) &&
	    drive.gpt.modified == (GPT_MODIFIED_HEADER2 | GPT_MODIFIED_ENTRIES2)) {
		printf("Drive size expansion detected; headers update required.\n");

		if (DriveClose(&drive, 1) != CGPT_OK)
			return CGPT_FAILED;
		if (DriveOpen(params->drive_name, &drive, O_RDWR,
				params->drive_size) != CGPT_OK)
			return CGPT_FAILED;

		header1 = (GptHeader *)(drive.gpt.primary_header);
		header2 = (GptHeader *)(drive.gpt.secondary_header);
		header1->alternate_lba = header2->my_lba;
		header1->last_usable_lba = header2->last_usable_lba
					= DriveLastUsableLBA(&drive);
		drive.gpt.modified = GPT_MODIFIED_HEADER1 | GPT_MODIFIED_HEADER2;
		UpdateCrc(&drive.gpt);
		printf("Primary Header is updated.\n");
		printf("Secondary Header is updated.\n");
	}
  return DriveClose(&drive, 1);
}
