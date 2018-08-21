#!/bin/bash -eux
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me=${0##*/}
TMP="$me.tmp"

# Include /usr/sbin for flahsrom(8)
PATH=/usr/sbin:"${PATH}"

# Test data files
DATA_LINK_BIOS="${SCRIPTDIR}/data/bios_link_mp.bin"
DATA_PEPPY_BIOS="${SCRIPTDIR}/data/bios_peppy_mp.bin"
LINK_VERSION="Google.Link.2695.1.133"
PEPPY_VERSION="Google.Peppy.4389.89.0"
LINK_HWID="X86 LINK TEST 6638"
PEPPY_HWID="X86 PEPPY TEST 4211"

# Work in scratch directory
cd "$OUTDIR"
set -o pipefail

# Prepare Files.
LINK_BIOS=${TMP}.src.link
PEPPY_BIOS=${TMP}.src.peppy

patch_file() {
	local file="$1"
	local section="$2"
	local data="$3"

	# NAME OFFSET SIZE
	local fmap_info="$(${FUTILITY} dump_fmap -p ${file} ${section})"
	local offset="$(echo "${fmap_info}" | sed 's/^[^ ]* //; s/ [^ ]*$//')"
	echo "offset: ${offset}"
	echo -n "${data}" | dd of="${file}" bs=1 seek="${offset}" conv=notrunc
}

cp -f ${DATA_LINK_BIOS} ${LINK_BIOS}
cp -f ${DATA_PEPPY_BIOS} ${PEPPY_BIOS}

# We want to test update using PEPPY_BIOS and LINK_BIOS. However, the platform
# check will not allow that due to Google_Link != Google_Peppy so we have to
# hack to FWID first, by replacing "Google_" to "Google.".
patch_file ${LINK_BIOS} RW_FWID_A Google.
patch_file ${LINK_BIOS} RW_FWID_B Google.
patch_file ${LINK_BIOS} RO_FRID Google.
patch_file ${PEPPY_BIOS} RW_FWID_A Google.
patch_file ${PEPPY_BIOS} RW_FWID_B Google.
patch_file ${PEPPY_BIOS} RO_FRID Google.

mkdir -p "${TMP}.unpack"
(cd "${TMP}.unpack"; "${FUTILITY}" dump_fmap -x "../${LINK_BIOS}")

# Hack LINK_BIOS so it has same root key as PEPPY_BIOS.
"${FUTILITY}" gbb -g --rootkey="${TMP}.rootkey.link" "${LINK_BIOS}"
"${FUTILITY}" gbb -g --rootkey="${TMP}.rootkey.peppy" "${PEPPY_BIOS}"
cp -f "${LINK_BIOS}" "${LINK_BIOS}.link_root_key"
"${FUTILITY}" gbb -s --rootkey="${TMP}.rootkey.peppy" "${LINK_BIOS}"

cp -f "${PEPPY_BIOS}" "${TMP}.expected.full"
cp -f "${PEPPY_BIOS}" "${TMP}.expected.rw"
cp -f "${PEPPY_BIOS}" "${TMP}.expected.a"
cp -f "${PEPPY_BIOS}" "${TMP}.expected.b"
"${FUTILITY}" gbb -s --hwid="${LINK_HWID}" "${TMP}.expected.full"
"${FUTILITY}" load_fmap "${TMP}.expected.full" \
	RW_VPD:${TMP}.unpack/RW_VPD \
	RO_VPD:${TMP}.unpack/RO_VPD
"${FUTILITY}" load_fmap "${TMP}.expected.rw" \
	RW_SECTION_A:${TMP}.unpack/RW_SECTION_A \
	RW_SECTION_B:${TMP}.unpack/RW_SECTION_B \
	RW_SHARED:${TMP}.unpack/RW_SHARED \
	RW_LEGACY:${TMP}.unpack/RW_LEGACY
"${FUTILITY}" load_fmap "${TMP}.expected.a" \
	RW_SECTION_A:${TMP}.unpack/RW_SECTION_A
"${FUTILITY}" load_fmap "${TMP}.expected.b" \
	RW_SECTION_B:${TMP}.unpack/RW_SECTION_B

test_update() {
	local test_name="$1"
	local emu_src="$2"
	local expected="$3"
	local error_msg="${expected#!}"
	local msg

	shift 3
	cp -f "${emu_src}" "${TMP}.emu"
	echo "*** Test Item: ${test_name}"
	if [ "${error_msg}" != "${expected}" ] && [ -n "${error_msg}" ]; then
		msg="$(! "${FUTILITY}" update --emulate "${TMP}.emu" "$@" 2>&1)"
		echo "${msg}" | grep -qF -- "${error_msg}"
	else
		"${FUTILITY}" update --emulate "${TMP}.emu" "$@"
		cmp "${TMP}.emu" "${expected}"
	fi
}

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
test "$(test_sys_props " , 4,")" = "1,4, "

# --sys_props: mainfw_act, tpm_fwver, is_vboot, wp_hw, wp_sw

# Test Full update.
test_update "Full update" \
	"${LINK_BIOS}" "${TMP}.expected.full" \
	-i "${PEPPY_BIOS}" --sys_props 0,0x10001,1 --wp=0

test_update "Full update (incompatible platform)" \
	"${LINK_BIOS}" "!platform is not compatible" \
	-i "${DATA_PEPPY_BIOS}" --sys_props 0,0x10001,1 --wp=0

# Test RW-only update.
test_update "RW update" \
	"${PEPPY_BIOS}" "${TMP}.expected.rw" \
	-i "${LINK_BIOS}" --sys_props 0,0x10001,1 --wp=1

test_update "RW update (incompatible platform)" \
	"${LINK_BIOS}" "!platform is not compatible" \
	-i "${DATA_PEPPY_BIOS}" --sys_props 0,0x10001,1 --wp=1

test_update "RW update (incompatible rootkey)" \
	"${PEPPY_BIOS}" "!Root keys do not match" \
	-i "${LINK_BIOS}.link_root_key" --sys_props 0,0x10001,1 --wp=1

# Test Try-RW update (vboot2). Link MP TPM ver is 0x10004.
test_update "RW update (A->B)" \
	"${PEPPY_BIOS}" "${TMP}.expected.b" \
	-i "${LINK_BIOS}" --sys_props 0,0x10001,1 --wp=1 -t

test_update "RW update (B->A)" \
	"${PEPPY_BIOS}" "${TMP}.expected.a" \
	-i "${LINK_BIOS}" --sys_props 1,0x10004,1 --wp=1 -t

test_update "RW update (TPM Anti-rollback: data key)" \
	"${PEPPY_BIOS}" "!Data key version rollback detected (2->1)" \
	-i "${LINK_BIOS}" --sys_props 1,0x20001,1 --wp=1 -t

test_update "RW update (TPM Anti-rollback: kernel key)" \
	"${PEPPY_BIOS}" "!Firmware version rollback detected (5->4)" \
	-i "${LINK_BIOS}" --sys_props 1,0x10005,1 --wp=1 -t

# Test Try-RW update (vboot1).
test_update "RW update (vboot1, A->B)" \
	"${PEPPY_BIOS}" "${TMP}.expected.b" \
	-i "${LINK_BIOS}" --sys_props 0,0x10001,0 --wp=1 -t
test_update "RW update (vboot1, B->B)" \
	"${PEPPY_BIOS}" "${TMP}.expected.b" \
	-i "${LINK_BIOS}" --sys_props 1,0x10001,0 --wp=1 -t
