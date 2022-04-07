#!/bin/bash -eux
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me="${0##*/}"
TMP="${me}.tmp"

# Work in scratch directory
cd "$OUTDIR"

KEYDIR="${SRCDIR}/tests/devkeys"
DATADIR="${SCRIPT_DIR}/futility/data"

# The input BIOS images are all signed with MP keys. We resign them with dev
# keys, which means we can precalculate the expected results. Note that the
# script does not change the root or recovery keys in the GBB.
INFILES="
${DATADIR}/bios_link_mp.bin
${DATADIR}/bios_peppy_mp.bin
"

# BIOS image containing CBFS RW/A and RW/B, and signed with developer keys.
GOOD_CBFS="${DATADIR}/bios_voxel_dev.bin"

# BIOS image containing CBFS RW/A and RW/B, and signed with developer keys.
INFILES="${INFILES}
${GOOD_CBFS}
"

# We also want to test that we can sign an image without any valid firmware
# preambles. That one won't be able to tell how much of the FW_MAIN region is
# the valid firmware, so it'll have to sign the entire region.
GOOD_VBLOCKS="${DATADIR}/bios_peppy_mp.bin"
ONEMORE=bios_peppy_mp_no_vblock.bin
CLEAN_B=bios_peppy_mp_clean_b_slot.bin
cp "${GOOD_VBLOCKS}" "${ONEMORE}"
cp "${GOOD_VBLOCKS}" "${CLEAN_B}"

NO_B_SLOT_PATCH="${DATADIR}/bios_voxel_dev.no_b_slot.xxd.patch"

BAD_KEYBLOCK_PATCHES="
${DATADIR}/bios_peppy_mp.bad_keyblock_data_key_size_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_keyblock_data_key_size_small.xxd.patch
${DATADIR}/bios_peppy_mp.bad_keyblock_size_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_keyblock_size_small.xxd.patch
"

INVALID_VBLOCK_PATCHES="
${DATADIR}/bios_peppy_mp.bad_preamble_body_sig_offset_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_body_sig_offset_small.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_body_sig_size_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_body_sig_size_small.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_sig_size_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_sig_size_small.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_size_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_size_small.xxd.patch
"

BAD_PREAMBLE_SIG_OFFSET_PATCHES="
${DATADIR}/bios_peppy_mp.bad_preamble_sig_offset_big.xxd.patch
${DATADIR}/bios_peppy_mp.bad_preamble_sig_offset_small.xxd.patch
"

BAD_PREAMBLE_BODY_DATA_SIZE_BIG_PATCH=\
  "${DATADIR}/bios_peppy_mp.bad_preamble_body_data_size_big.xxd.patch"
BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_PATCH=\
  "${DATADIR}/bios_peppy_mp.bad_preamble_body_data_size_small.xxd.patch"

"${FUTILITY}" load_fmap "${ONEMORE}" VBLOCK_A:/dev/urandom VBLOCK_B:/dev/zero
INFILES="${INFILES} ${ONEMORE}"

# args: xxd_patch_file input_file
function apply_xxd_patch {
	xxd -r "${1}" "${2}"
}

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
  [ "${m}" = "4" ]

  # check the sha1sums
  "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${outfile}" \
    | grep sha1sum \
    | sed -e 's/.*: \+//' > "${TMP}.${base}.sha.new"
  cmp "${SCRIPT_DIR}/futility/data_${base}_expect.txt" "${TMP}.${base}.sha.new"

   # and the LOEM stuff
   "${FUTILITY}" dump_fmap -x "${outfile}" \
     "FW_MAIN_A:${loemdir}/fw_main_A" "FW_MAIN_B:${loemdir}/fw_main_B"

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
GOOD_CBFS_OUT="${TMP}.${GOOD_CBFS##*/}.new"

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

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${GOOD_CBFS_OUT}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.good_cbfs.body"
"${FUTILITY}" dump_fmap -p "${GOOD_CBFS_OUT}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.good_cbfs.fw_main"
if cmp "${TMP}.good_cbfs.body" "${TMP}.good_cbfs.fw_main"; then false; fi


# Sign CBFS image after adding new files. Size should increase but, still be
# smaller than FlashMap size.
: $(( count++ ))
echo -n "${count} " 1>&3

cp "${GOOD_CBFS_OUT}" "${GOOD_CBFS_OUT}.1"
head -c 512 /dev/zero > "${TMP}.zero_512"
cbfstool "${GOOD_CBFS_OUT}.1" expand -r FW_MAIN_A,FW_MAIN_B
cbfstool "${GOOD_CBFS_OUT}.1" add \
  -r FW_MAIN_A,FW_MAIN_B -f "${TMP}.zero_512" -n new-data-file -t raw

"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${GOOD_CBFS_OUT}.1"

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${GOOD_CBFS_OUT}.1" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.good_cbfs.1.body"
"${FUTILITY}" dump_fmap -p "${GOOD_CBFS_OUT}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.good_cbfs.1.fw_main"

# Check if size increased, but also if it was corectly truncated, so it does not
# span over whole FlashMap area.
[[ $(head -n1 "${TMP}.good_cbfs.body") \
  < $(head -n1 "${TMP}.good_cbfs.1.body") ]]
[[ $(tail -n1 "${TMP}.good_cbfs.body") \
  < $(tail -n1 "${TMP}.good_cbfs.1.body") ]]
[[ $(head -n1 "${TMP}.good_cbfs.1.body") \
  < $(head -n1 "${TMP}.good_cbfs.1.fw_main") ]]
[[ $(tail -n1 "${TMP}.good_cbfs.1.body") \
  < $(tail -n1 "${TMP}.good_cbfs.1.fw_main") ]]


# Sign image again but don't specify the version or the preamble flags.
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
[ "${m}" = "4" ]


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


# Check signing when B slot is empty
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
# of FW_MAIN_B should be taken from FlashMap.
if cmp "${TMP}.clean_b.body" "${TMP}.clean_b.fw_main" ; then false; fi
if cmp "${TMP}.clean_b.body" "${TMP}.good.body" ; then false; fi
cmp <(head -n1 "${TMP}.clean_b.body") <(head -n1 "${TMP}.good.body")
cmp <(tail -n1 "${TMP}.clean_b.body") <(tail -n1 "${TMP}.clean_b.fw_main")

# Version for slot A should be kept intact, while for B slot it should default
# to 1. All flags should be zero.
m=$(( $("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
      "${CLEAN_B}.1" | grep -c -E 'Firmware version: +1$|Preamble flags: +0$')
    + $("${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
      "${CLEAN_B}.1" | grep -c -E 'Firmware version: +2$') ))
[ "${m}" = "4" ]

# Check signing when there is no B slot
: $(( count++ ))
echo -n "${count} " 1>&3

NO_B_SLOT="${TMP}.${GOOD_CBFS##*/}.no_b_slot"
NO_B_SLOT_SIGNED_IMG="${NO_B_SLOT}.signed"

cp "${GOOD_CBFS}" "${NO_B_SLOT}"
apply_xxd_patch "${NO_B_SLOT_PATCH}" "${NO_B_SLOT}"

"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  -v 1 \
  "${NO_B_SLOT}" "${NO_B_SLOT_SIGNED_IMG}"

"${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${NO_B_SLOT_SIGNED_IMG}" \
  | awk '/Firmware body size:/ {print $4}' > "${TMP}.no_b_slot.body"
"${FUTILITY}" dump_fmap -p "${NO_B_SLOT_SIGNED_IMG}" \
  | awk '/FW_MAIN_/ {print $3}' > "${TMP}.no_b_slot.fw_main"

if cmp "${TMP}.no_b_slot.body" "${TMP}.no_b_slot.fw_main" ; then false; fi
cmp "${TMP}.no_b_slot.body" <(tail -n1 "${TMP}.good_cbfs.body")

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
# FW_MAIN_B size. With this behavior futility should fail to sign this image,
# as cbfstool should never return incorrect sile (larger than area).
cp "${GOOD_CBFS}" "${TMP}.good_cbfs.bin"
FW_MAIN_A_SIZE="$(printf '0x%x' \
  "$(cbfstool "${TMP}.good_cbfs.bin" truncate -r FW_MAIN_A)")"
MARK_FILE="$(realpath "${TMP}.mark1")"
rm -f "${MARK_FILE}"

cat << EOF > "${CBFSTOOL_STUB}"
#!/usr/bin/env bash
if ! [ -f "${MARK_FILE}" ]; then
  echo "${FW_MAIN_A_SIZE}";
  echo 1 > "${MARK_FILE}";
else
  echo 0xFFFFAA0;
fi
exit 0;
EOF

if CBFSTOOL="${CBFSTOOL_STUB}" "${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  -v 1 \
  "${GOOD_CBFS}" "${TMP}.2.${GOOD_CBFS##*/}"
then
  false
fi


# Check various incorrect values in VBLOCK (keyblock and preamble)
: $(( count++ ))
echo -n "${count} " 1>&3

bad_counter=0
for keyblock_patch in $BAD_KEYBLOCK_PATCHES; do
  BAD_KEYBLOCK_IN="${GOOD_OUT}.bad.${bad_counter}.in.bin"
  BAD_KEYBLOCK_OUT="${GOOD_OUT}.bad.${bad_counter}.out.bin"
  cp "${GOOD_OUT}" "${BAD_KEYBLOCK_IN}"
  apply_xxd_patch "${keyblock_patch}" "${BAD_KEYBLOCK_IN}"

  if "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
     "${BAD_KEYBLOCK_IN}" \
     | grep -q -E 'VBLOCK_A keyblock component is invalid'
  then
    false
  fi

  "${FUTILITY}" sign \
    -s "${KEYDIR}/firmware_data_key.vbprivk" \
    -b "${KEYDIR}/firmware.keyblock" \
    -k "${KEYDIR}/kernel_subkey.vbpubk" \
    "${BAD_KEYBLOCK_IN}" "${BAD_KEYBLOCK_OUT}" \
    2>&1 >/dev/null \
    | grep -q -E 'VBLOCK_A keyblock is invalid'

  "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
    "${BAD_KEYBLOCK_OUT}" \
    | awk '/Firmware body size:/ {print $4}' 1>&2 > "${BAD_KEYBLOCK_OUT}.body"
  "${FUTILITY}" dump_fmap -p "${BAD_KEYBLOCK_OUT}" \
    | awk '/FW_MAIN_/ {print $3}' 1>&2 > "${BAD_KEYBLOCK_OUT}.fw_main"

  cmp "${BAD_KEYBLOCK_OUT}.fw_main" "${TMP}.good.fw_main"
  cmp <(head -n1 "${BAD_KEYBLOCK_OUT}.body") <(head -n1 "${TMP}.good.fw_main")
  cmp <(tail -n1 "${BAD_KEYBLOCK_OUT}.body") <(tail -n1 "${TMP}.good.body")

  : $(( bad_counter++ ))
done

for vblock_patch in $INVALID_VBLOCK_PATCHES; do
  BAD_VBLOCK_IN="${GOOD_OUT}.bad.${bad_counter}.in.bin"
  BAD_VBLOCK_OUT="${GOOD_OUT}.bad.${bad_counter}.out.bin"
  cp "${GOOD_OUT}" "${BAD_VBLOCK_IN}"
  apply_xxd_patch "${vblock_patch}" "${BAD_KEYBLOCK_IN}"

  if "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
       "${BAD_VBLOCK_IN}" \
       | grep -q -E 'VBLOCK_A is invalid'
  then
    false
  fi

  "${FUTILITY}" sign \
    -s "${KEYDIR}/firmware_data_key.vbprivk" \
    -b "${KEYDIR}/firmware.keyblock" \
    -k "${KEYDIR}/kernel_subkey.vbpubk" \
    "${BAD_VBLOCK_IN}" "${BAD_VBLOCK_OUT}"

  : $(( bad_counter++ ))
done

for bpso_patch in $BAD_PREAMBLE_SIG_OFFSET_PATCHES; do
  BAD_IN="${GOOD_OUT}.bad.${bad_counter}.in.bin"
  BAD_OUT="${GOOD_OUT}.bad.${bad_counter}.out.bin"
  cp "${GOOD_OUT}" "${BAD_IN}"
  apply_xxd_patch "${bpso_patch}" "${BAD_IN}"

  if "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" "${BAD_IN}" \
       | grep -q -E 'VBLOCK_A is invalid'
  then
    false
  fi

  if "${FUTILITY}" sign \
       -s "${KEYDIR}/firmware_data_key.vbprivk" \
       -b "${KEYDIR}/firmware.keyblock" \
       -k "${KEYDIR}/kernel_subkey.vbpubk" \
       "${BAD_IN}" "${BAD_OUT}" \
       | grep -q -E 'VBLOCK_A is invalid'
  then
    false
  fi
  : $(( bad_counter++ ))
done

BAD_PREAMBLE_BODY_DATA_SIZE_BIG_IN="${GOOD_OUT}.bad.${bad_counter}.in.bin"
BAD_PREAMBLE_BODY_DATA_SIZE_BIG_OUT="${GOOD_OUT}.bad.${bad_counter}.out.bin"
cp "${GOOD_OUT}" "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_IN}"
apply_xxd_patch "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_PATCH}" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_IN}"

if "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
     "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_IN}" \
     | grep -q -E 'VBLOCK_A is invalid'
then
  false
fi

"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_IN}" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_BIG_OUT}" \
  2>&1 | grep -q -E 'VBLOCK_A says the firmware is larger than we have'
: $(( bad_counter++ ))

BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_IN="${GOOD_OUT}.bad.${bad_counter}.in.bin"
BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_OUT="${GOOD_OUT}.bad.${bad_counter}.out.bin"
cp "${GOOD_OUT}" "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_IN}"
apply_xxd_patch "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_PATCH}" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_IN}"

if "${FUTILITY}" verify --publickey "${KEYDIR}/root_key.vbpubk" \
     "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_IN}" \
     | grep -q -E 'VBLOCK_A is invalid'
then
  false
fi

"${FUTILITY}" sign \
  -s "${KEYDIR}/firmware_data_key.vbprivk" \
  -b "${KEYDIR}/firmware.keyblock" \
  -k "${KEYDIR}/kernel_subkey.vbpubk" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_IN}" \
  "${BAD_PREAMBLE_BODY_DATA_SIZE_SMALL_OUT}"
: $(( bad_counter++ ))


# cleanup
rm -rf "${TMP}"* "${ONEMORE}"
exit 0
