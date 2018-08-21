#!/bin/bash -eux
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me=${0##*/}
TMP="$me.tmp"

# Include /usr/sbin for flahsrom(8)
PATH=/usr/sbin:"${PATH}"

# Test data files
PEPPY_BIOS="${SCRIPTDIR}/data/bios_peppy_mp.bin"
LINK_BIOS="${SCRIPTDIR}/data/bios_link_mp.bin"
LINK_VERSION="Google_Link.2695.1.133"
PEPPY_VERSION="Google_Peppy.4389.89.0"

# Work in scratch directory
cd "$OUTDIR"
set -o pipefail

# Prepare temporary files.
cp -f "${LINK_BIOS}" "${TMP}.emu"

# Test command execution.
output="$("${FUTILITY}" update -i "${PEPPY_BIOS}" --emulate "${TMP}.emu")"
target="$(echo "${output}" | sed -n 's/^>> Target.*(//p' | sed 's/).*//')"
current="$(echo "${output}" | sed -n 's/^>> Current.*(//p' | sed 's/).*//')"
expected1="RO:${PEPPY_VERSION}, RW/A:${PEPPY_VERSION}, RW/B:${PEPPY_VERSION}"
expected2="RO:${LINK_VERSION}, RW/A:${LINK_VERSION}, RW/B:${LINK_VERSION}"
test "${target}" = "${expected1}"
test "${current}" = "${expected2}"
cmp "${TMP}.emu" "${PEPPY_BIOS}"
