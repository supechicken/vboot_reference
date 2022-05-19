#!/bin/bash -eux
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me=${0##*/}
TMP="${me}.tmp"
count=0

# Work in scratch directory
cd "${OUTDIR}"

TEST_BIN="${SCRIPT_DIR}/futility/data/bios_peppy_mp.bin"

"${FUTILITY}" dump_fmap -F "${TEST_BIN}" > "${TMP}.bios_peppy_mp.fmap"
fmap_areas=$(wc -l "${TMP}.bios_peppy_mp.fmap")

: $(( count++ ))
echo -n "${count} " 1>&3
## Remove first entry
"${FUTILITY}" remove_fmap "${TEST_BIN}" -o "${TMP}.no_si_all.bin" SI_ALL
"${FUTILITY}" dump_fmap -F "${TMP}.no_si_all.bin" > "${TMP}.no_si_all.fmap"

if cmp "${TMP}.bios_peppy_mp.fmap" "${TMP}.no_si_all.fmap"; then false; fi
[ "$(( fmap_areas - 1 ))" = "$(wc -l \"${TMP}.no_si_all.fmap\")" ]

tail -n +2 "${TMP}.bios_peppy_mp.fmap" > "${TMP}.bios_peppy_mp.fmap.1"
cmp "${TMP}.bios_peppy_mp.fmap.1" "${TMP}.no_si_all.fmap"

: $(( count++ ))
echo -n "${count} " 1>&3
## Remove last entry
"${FUTILITY}" remove_fmap "${TEST_BIN}" -o "${TMP}.no_boot_stub.bin" BOOT_STUB
"${FUTILITY}" dump_fmap -F "${TMP}.no_boot_stub.bin" > "${TMP}.no_boot_stub.fmap"

if cmp "${TMP}.bios_peppy_mp.fmap" "${TMP}.no_boot_stub.fmap"; then false; fi
[ "$(( fmap_areas - 1 ))" = $(wc -l "${TMP}.no_boot_stub.fmap") ]

head -n -1 "${TMP}.bios_peppy_mp.fmap" > "${TMP}.bios_peppy_mp.fmap.2"
cmp "${TMP}.bios_peppy_mp.fmap.2" "${TMP}.no_boot_stub.fmap"


: $(( count++ ))
echo -n "${count} " 1>&3
## Remove two entries
"${FUTILITY}" remove_fmap "${TEST_BIN}" \
	-o "${TMP}.bios_peppy_mp.no_b_slot.bin" FW_MAIN_B VBLOCK_B
"${FUTILITY}" dump_fmap -F "${TMP}.bios_peppy_mp.no_b_slot.bin" \
	> "${TMP}.no_b_slot.fmap"

# FlashMaps should not be the same
if cmp "${TMP}.bios_peppy_mp.fmap" "${TMP}.no_b_slot.fmap"; then false; fi
[ "$(( fmap_areas - 2 ))" = $(wc -l "${TMP}.no_b_slot.fmap") ]

grep -v -E '\b(FW_MAIN_B|VBLOCK_B)\b' \
  "${TMP}.bios_peppy_mp.fmap" > "${TMP}.bios_peppy_mp.fmap.3"
cmp "${TMP}.bios_peppy_mp.fmap.3" "${TMP}.no_b_slot.fmap"


# cleanup
rm -rf ${TMP}*
exit 0
