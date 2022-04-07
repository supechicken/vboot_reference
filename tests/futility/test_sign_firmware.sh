#!/bin/bash -eux
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me="${0##*/}"
TMP="${me}.tmp"

# Work in scratch directory
cd "$OUTDIR"

KEYDIR="${SRCDIR}/tests/devkeys"

# The input BIOS images are all signed with MP keys. We resign them with dev
# keys, which means we can precalculate the expected results. Note that the
# script does not change the root or recovery keys in the GBB.
INFILES="
${SCRIPT_DIR}/futility/data/bios_link_mp.bin
${SCRIPT_DIR}/futility/data/bios_peppy_mp.bin
"

# BIOS image containing CBFS RW/A and RW/B, and signed with developer keys.
GOOD_CBFS="${SCRIPT_DIR}/futility/data/bios_voxel_dev.bin"

# BIOS image with FW_MAIN_B and VBLOCK_B removed from FlashMap
NO_B_SLOT="${SCRIPT_DIR}/futility/data/bios_voxel_dev.no_b_slot.bin"

# BIOS image containing CBFS RW/A and RW/B, and signed with developer keys.
INFILES="${INFILES}
${GOOD_CBFS}
"

# We also want to test that we can sign an image without any valid firmware
# preambles. That one won't be able to tell how much of the FW_MAIN region is
# the valid firmware, so it'll have to sign the entire region.
GOOD_VBLOCKS="${SCRIPT_DIR}/futility/data/bios_peppy_mp.bin"
ONEMORE=bios_peppy_mp_no_vblock.bin
CLEAN_B=bios_peppy_mp_clean_b_slot.bin
cp "${GOOD_VBLOCKS}" "${ONEMORE}"
cp "${GOOD_VBLOCKS}" "${CLEAN_B}"


"${FUTILITY}" load_fmap "${ONEMORE}" VBLOCK_A:/dev/urandom VBLOCK_B:/dev/zero
INFILES="${INFILES} ${ONEMORE}"

set -o pipefail

count=0
for infile in $INFILES; do

  base=${infile##*/}

  : $(( count++ ))
  echo -n "${count} " 1>&3

  outfile="${TMP}.${base}.new"
  loemid="loem"
  loemdir="${TMP}.${base}_dir"

  mkdir -p "${loemdir}"

  "${FUTILITY}" sign \
    -s "${KEYDIR}/firmware_data_key.vbprivk" \
    -b "${KEYDIR}/firmware.keyblock" \
    -k "${KEYDIR}/kernel_subkey.vbpubk" \
    -v 14 \
    -f 8 \
    -d "${loemdir}" \
    -l "${loemid}" \
    "${infile}" "${outfile}"

  # check the firmware version and preamble flags
  m=$("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
        "${outfile}" | grep -c -E 'Firmware version: +14$|Preamble flags: +8$')
  [ "$m" = "4" ]

  # check the sha1sums
  "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${outfile}" \
    | grep sha1sum \
    | sed -e 's/.*: \+//' > "${TMP}.${base}.sha.new"
  cmp "${SCRIPT_DIR}/futility/data_${base}_expect.txt" "${TMP}.${base}.sha.new"

   # and the LOEM stuff
   "${FUTILITY}" dump_fmap -x "${outfile}" \
     "FW_MAIN_A:${loemdir}/fw_main_A" "FW_MAIN_B:${loemdir}/fw_main_B" \
     "Firmware A Data:${loemdir}/fw_main_A" \
     "Firmware B Data:${loemdir}/fw_main_B"


   "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
     --fv "${loemdir}/fw_main_A" \
     "${loemdir}/vblock_A.${loemid}" | grep sha1sum \
     | sed -e 's/.*: \+//' > "${loemdir}/loem.sha.new"
   "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
     --fv "${loemdir}/fw_main_B" \
     "${loemdir}/vblock_B.${loemid}" | grep sha1sum \
     | sed -e 's/.*: \+//' >> "${loemdir}/loem.sha.new"

  # the vblocks don't have root or recovery keys
  tail -4 "${SCRIPT_DIR}/futility/data_${base}_expect.txt" \
    > "${loemdir}/sha.expect"
  cmp "${loemdir}/sha.expect" "${loemdir}/loem.sha.new"

done

# Make sure that the BIOS with the good vblocks signed the right size.
GOOD_OUT="${TMP}.${GOOD_VBLOCKS##*/}.new"
MORE_OUT="${TMP}.${ONEMORE##*/}.new"

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${GOOD_OUT}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.good.body"
"${FUTILITY}" dump_fmap -p "${GOOD_OUT}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.good.fw_main"
# This should fail because they're different
if cmp "${TMP}.good.body" "${TMP}.good.fw_main"; then false; fi

# Make sure that the BIOS with the bad vblocks signed the whole fw body
"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${MORE_OUT}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.onemore.body"
"${FUTILITY}" dump_fmap -p "${MORE_OUT}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.onemore.fw_main"
# These should match
cmp "${TMP}.onemore.body" "${TMP}.onemore.fw_main"
cmp "${TMP}.onemore.body" "${TMP}.good.fw_main"


# Sign the last one again but don't specify the version or the preamble flags.
# The firmware version and preamble flags should be preserved.
# NOTICE: Version preservation behavior changed from defaulting to 1.
: $(( count++ ))
echo -n "${count} " 1>&3

"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${MORE_OUT}" "${MORE_OUT}.2"

m=$("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
      "${MORE_OUT}.2" | grep -c -E 'Firmware version: +14$|Preamble flags: +8$')
[ "$m" = "4" ]


# If the original preamble is not present, the preamble flags should be zero.
: $(( count++ ))
echo -n "${count} " 1>&3

"${FUTILITY}" load_fmap "${MORE_OUT}" VBLOCK_A:/dev/urandom VBLOCK_B:/dev/zero
"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${MORE_OUT}" "${MORE_OUT}.3"

m=$("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
      "${MORE_OUT}.3" | grep -c -E 'Firmware version: +1$|Preamble flags: +0$')
[ "${m}" = "4" ]

# Check signing when B slot is zero
: $(( count++ ))
echo -n "${count} " 1>&3

"${FUTILITY}" load_fmap "${CLEAN_B}" VBLOCK_B:/dev/zero FW_MAIN_B:/dev/zero
"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${CLEAN_B}" "${CLEAN_B}.1"

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${CLEAN_B}.1" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.clean_b.body"
"${FUTILITY}" dump_fmap -p "${CLEAN_B}.1" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.clean_b.fw_main"

# These should not be equal, as FW_MAIN_A size should be kept intact, when size
# of FW_MAIN_B should be taken from FlashMap
if cmp "${TMP}.clean_b.body" "${TMP}.clean_b.fw_main" ; then false; fi
if cmp "${TMP}.clean_b.body" "${TMP}.good.body" ; then false; fi
cmp <(head -n1 "${TMP}.clean_b.body") <(head -n1 "${TMP}.good.body")
cmp <(tail -n1 "${TMP}.clean_b.body") <(tail -n1 "${TMP}.clean_b.fw_main")

# Check signing when there is no B slot
: $(( count++ ))
echo -n "${count} " 1>&3

NO_B_SLOT_SIGNED_IMG="${TMP}.${NO_B_SLOT##*/}"
"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  -v 1 \
  "${NO_B_SLOT}" "${NO_B_SLOT_SIGNED_IMG}"

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${GOOD_CBFS}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.good_cbfs.body"
"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${NO_B_SLOT_SIGNED_IMG}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.no_b_slot.body"
"${FUTILITY}" dump_fmap -p "${NO_B_SLOT_SIGNED_IMG}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.no_b_slot.fw_main"

if cmp "${TMP}.no_b_slot.body" "${TMP}.no_b_slot.fw_main" ; then false; fi
cmp "${TMP}.no_b_slot.body" <(tail -n1 "${TMP}.good_cbfs.body") #FIXHIR

m=$("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${NO_B_SLOT_SIGNED_IMG}" \
  | grep -c -E 'Firmware version: +1$|Preamble flags: +0$')
[ "${m}" = "2" ]

# Check signing when cbfstool reports incorrect size
# Signing should fail, as it should not be possible for CBFS contents to be
# bigger than FlashMap size of the area
: $(( count++ ))
echo -n "${count} " 1>&3

CBFSTOOL_STUB="$(realpath "${TMP}.cbfs_stub.sh")"
echo -en 'echo "0xFFEEDD0"; exit 0;' > "${CBFSTOOL_STUB}"
chmod +x "${CBFSTOOL_STUB}"

if CBFSTOOL="${CBFSTOOL_STUB}" "${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  -v 1 \
  "${GOOD_CBFS}" "${TMP}.1.${GOOD_CBFS##*/}"
then
  false
fi

# Redefine cbfstool stub to return valid value for FW_MAIN_A and invalid for
# FW_MAIN_B size
cp "${GOOD_CBFS}" "${TMP}.good_cbfs.bin"
FW_MAIN_A_SIZE="$(printf '0x%x' \
  "$(cbfstool "${TMP}.good_cbfs.bin" truncate -r FW_MAIN_A)")"
MARK_FILE="$(realpath "${TMP}.mark1")"

cat << EOF > "${CBFSTOOL_STUB}"
#!/usr/bin/env bash
if ! [ -f "${MARK_FILE}" ]; then
  echo "${FW_MAIN_A_SIZE}";
  echo 1 > "${MARK_FILE}";
else
  echo 0xFFFFAA0;
  rm "${MARK_FILE}";
fi
exit 0;
EOF

CBFSTOOL="${CBFSTOOL_STUB}" "${FUTILITY}" --debug sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  -v 1 \
  "${GOOD_CBFS}" "${TMP}.2.${GOOD_CBFS##*/}"


# cleanup
rm -rf "${TMP}"* "${ONEMORE}" "${CBFSTOOL_STUB}"
exit 0
