#!/bin/bash -eux
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o pipefail

me="${0##*/}"
TMP="$me.tmp"

# Set to 1 to update the expected output
UPDATE_MODE=0

# Test 'futility show' and 'futility verify'
# Entry: <file> <error_level> <extra_options>
#   file: Input file
#   error_level:
#     0: Both 'futility show' and 'futility verify' expected to succeed
#     1: 'show' expected to succeed, but 'verify' expected to fail
#     2: Both 'show' and 'verify' expected to fail
#   extra_options: Extra options passed to 'futility show' or 'futility verify'
ENTRIES=(
  ## [type] pubkey/prikey
  "tests/devkeys/root_key.vbpubk 0"
  "tests/devkeys/root_key.vbprivk 0"
  ## [type] pubkey21/prikey21
  "tests/futility/data/sample.vbpubk2 0"
  "tests/futility/data/sample.vbprik2 0"
  ## [type] pem
  "tests/testkeys/key_rsa2048.pem 0"
  "tests/testkeys/key_rsa8192.pub.pem 0"
  ## [type] keyblock
  "tests/devkeys/kernel.keyblock 1"
  ## [type] fw_pre
  "tests/futility/data/fw_vblock.bin 1"
  ## [type] gbb
  "tests/futility/data/fw_gbb.bin 0"
  ## [type] bios
  # valid bios without VBOOT_CBFS_INTEGRATION
  "tests/futility/data/bios_peppy_mp.bin 0"
  # bios with VBOOT_CBFS_INTEGRATION; invalid metadata hash in VBLOCK_B
  # TODO(b/290287265): error_level should be 1
  "tests/futility/data/bios_coachz_cbfs.bin 2"
  # [type] kernel
  "tests/futility/data/kern_preamble.bin 1"
)

PARSEABLE_UNSUPPORTED_TYPES=( "pubkey21" "prikey21" "pem" )

support_parseable()
{
  local file_type="$1"
  for t in "${PARSEABLE_UNSUPPORTED_TYPES[@]}"; do
    if [ "$t" = "${file_type}" ] ; then
      return 1
    fi
  done
  return 0
}

check_output()
{
  local subcmd="$1"
  local file="$2"
  local parseable="$3"
  local cmd=( "${FUTILITY}" "${subcmd}" "${file}" )
  local outfile
  local gotfile
  local wantfile

  if [ "${parseable}" -gt 0 ]; then
    cmd+=( "-P" )
    outfile="show.parseable.${file//\//_}"
  else
    outfile="show.${file//\//_}"
  fi
  gotfile="${OUTDIR}/${outfile}"
  wantfile="${SRCDIR}/tests/futility/expect_output/${outfile}"

  ( cd "${SRCDIR}" && "${cmd[@]}" ) | tee "${gotfile}"

  [[ "${UPDATE_MODE}" -gt 0 ]] && cp "${gotfile}" "${wantfile}"

  diff "${wantfile}" "${gotfile}"
}

for entry in "${ENTRIES[@]}"; do
  read -ra arr <<<"${entry}"
  file="${arr[0]}"
  level="${arr[1]}"
  opts=()
  if [ "${#arr[@]}" -ge 3 ]; then
    opts=("${arr[@]:2}")
  fi
  echo "FILE" "${file}"    options: "${opts[@]}"
  echo len "${#opts[@]}"

  succ_cmd=""
  fail_cmd=""
  if [ "${level}" -eq 0 ]; then
    succ_cmd="verify"
  elif [ "${level}" -eq 1 ]; then
    succ_cmd="show"
    fail_cmd="verify"
  else
      fail_cmd="show"
  fi

  if [ ! -z "${succ_cmd}" ]; then
    # Normal output
    check_output "${succ_cmd}" "${file}" 0

    # Parseable output
    file_type=$(cd "${SRCDIR}" && "${FUTILITY}" show "${file}" -t | cut -f2)
    if support_parseable "${file_type}"; then
      check_output "${succ_cmd}" "${file}" 1
    else
      echo "Skipped parseable output for ${file}"
    fi
  fi

  if [ ! -z "${fail_cmd}" ]; then
    ( cd "${SRCDIR}" && ! "${FUTILITY}" "${fail_cmd}" "${file}" )
  fi
done

# cleanup
rm -rf "${TMP}"*
exit 0
