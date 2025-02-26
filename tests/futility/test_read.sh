#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Skip test with USE_FLASHROM=0
"${FUTILITY}" help read 2>&1 | grep -vq 'built without flashrom support' || exit 0

set -eux

me=${0##*/}
TMP="${me}.tmp"

# Work in scratch directory
cd "${OUTDIR}"

PEPPY_BIOS="${SCRIPT_DIR}/futility/data/bios_peppy_mp.bin"

"${FUTILITY}" read --emulate="${PEPPY_BIOS}" "${TMP}"
cmp "${PEPPY_BIOS}" "${TMP}"

"${FUTILITY}" read --emulate="${PEPPY_BIOS}" --region="GBB,RO_VPD" "${TMP}"
! cmp --silent "${PEPPY_BIOS}" "${TMP}_GBB"
! cmp --silent "${PEPPY_BIOS}" "${TMP}_RO_VPD"

# cleanup
rm -f "${TMP}"*
exit 0
