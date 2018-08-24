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

# Test full update and image loading.
# Test command execution.
output="$(${FUTILITY} update -i "${PEPPY_BIOS}" --wp 0 --emulate "${TMP}.emu")"
target="$(echo "${output}" | sed -n 's/^>> Target.*(//p' | sed 's/).*//')"
current="$(echo "${output}" | sed -n 's/^>> Current.*(//p' | sed 's/).*//')"
expected1="RO:${PEPPY_VERSION}, RW/A:${PEPPY_VERSION}, RW/B:${PEPPY_VERSION}"
expected2="RO:${LINK_VERSION}, RW/A:${LINK_VERSION}, RW/B:${LINK_VERSION}"
test "${target}" = "${expected1}"
test "${current}" = "${expected2}"
cmp "${TMP}.emu" "${PEPPY_BIOS}"

# $TMP.emu is PEPPY now. Prepare new testing targets.
mkdir -p "${TMP}.unpack"
(cd "${TMP}.unpack"; "${FUTILITY}" dump_fmap -x "${LINK_BIOS}")
cp -f "${TMP}.emu" "${TMP}.expected.rw"
cp -f "${TMP}.emu" "${TMP}.expected.a"
cp -f "${TMP}.emu" "${TMP}.expected.b"
"${FUTILITY}" load_fmap "${TMP}.expected.rw" \
	RW_SECTION_A:${TMP}.unpack/RW_SECTION_A \
	RW_SECTION_B:${TMP}.unpack/RW_SECTION_B \
	RW_SHARED:${TMP}.unpack/RW_SHARED \
	RW_LEGACY:${TMP}.unpack/RW_LEGACY
"${FUTILITY}" load_fmap "${TMP}.expected.a" \
	RW_SECTION_A:${TMP}.unpack/RW_SECTION_A
"${FUTILITY}" load_fmap "${TMP}.expected.b" \
	RW_SECTION_B:${TMP}.unpack/RW_SECTION_B

# Test RW-only update.
"${FUTILITY}" update -i "${LINK_BIOS}" --emulate "${TMP}.emu" --wp 1
cmp "${TMP}.emu" "${TMP}.expected.rw"

# Test Try-RW update.
cp -f "${PEPPY_BIOS}" "${TMP}.emu"
"${FUTILITY}" update -i "${LINK_BIOS}" --emulate ${TMP}.emu --sys_props 0,1,1 -t
cmp "${TMP}.emu" "${TMP}.expected.b"
cp -f "${PEPPY_BIOS}" "${TMP}.emu"
"${FUTILITY}" update -i "${LINK_BIOS}" --emulate ${TMP}.emu --sys_props 1,1,1 -t
cmp "${TMP}.emu" "${TMP}.expected.a"

# Test --sys_props
test_sys_props() {
	! "${FUTILITY}" --debug update --sys_props "$*" |
		sed -n 's/.*property\[\(.*\)].value = \(.*\)/\1,\2,/p' |
		tr '\n' ' '
}

test "$(test_sys_props "1,2,3")" = "0,1, 1,2, 2,3, "
test "$(test_sys_props "1 2 3")" = "0,1, 1,2, 2,3, "
test "$(test_sys_props "1, 2,3 ")" = "0,1, 1,2, 2,3, "
test "$(test_sys_props "   1,, 2")" = "0,1, 2,2, "
test "$(test_sys_props " 4,")" = "0,4, "
