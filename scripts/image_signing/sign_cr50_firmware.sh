#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

load_shflags || exit 1

FLAGS_HELP="Usage: ${PROG} [options] <input_dir> <key_dir> <output_image>

Signs <input_dir> with keys in <key_dir>.
"

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error and uninitialized variables.
set -e
set -u

# This function accepts two arguments, names of two binary files.
#
# It searches the first passed in file for the first 8 bytes of the second
# passed in file. The od utility is used to generate full hex dump of the
# first file (16 bytes per line) and the first 8 bytes of the second file.
#
# grep is used to check if the pattern is present in the full dump. If the
# pattern is not found, the first file is dumped again, this time with an 8
# byte offset into the file. This makes sure that if the match is present, but
# is spanning two lines of the original hex dump, it is in a single dump line
# the second time around.
find_blob_in_blob() {
  local main_blob="${1}"
  local pattern_blob="${2}"
  local pattern
  local od_options="-An -tx1"

  # Get the first 8 bytes of the pattern blob.
  pattern="$(od ${od_options} -N8 "${pattern_blob}")"

  if od ${od_options} "${main_blob}" | grep "${pattern}" > /dev/null; then
    return 0
  fi

  # Just in case pattern was wrapped in the previous od output, let's do it
  # again with an 8 bytes offset
  if od ${od_options} -j8 "${main_blob}" |
      grep "${pattern}" > /dev/null; then
    return 0
  fi

  return 1
}

# This function accepts two arguments, names of the two elf files.
#
# The files are searched for test RMA public key patterns - x25519 or p256,
# both files are supposed to have pattern of one of these keys and not the
# other. If this holds true the function prints the public key base name. If
# not both files include the same key, or include more than one key, the
# function reports failure and exits the script.
determine_rma_key_base() {
  local base_name="${EC_ROOT}/board/cr50/rma_key_blob"
  local curve
  local curves=( "x25519" "p256" )
  local elf
  local elves=( "$1" "$2" )
  local key_file
  local mask=1
  local result=0

  for curve in ${curves[@]}; do
    key_file="${base_name}.${curve}.test"
    for elf in ${elves[@]}; do
      if find_blob_in_blob "${elf}" "${key_file}"; then
        result=$(( result | mask ))
      fi
      mask=$(( mask << 1 ))
    done
  done

  case "${result}" in
    (3)  curve="x25519";;
    (12) curve="p256";;
    (*) error "could not determine key type in the elves"
        exit 1
        ;;
  esac

  echo "${base_name}.${curve}"
}

# Sign cr50 RW firmware elf images into a combined cr50 firmware image
# using the provided production keys and manifests.
do_bs() {
  do_prod=

  local key_file="${1}"
  local manifest_file="${2}"
  local fuses_file="${3}"
  local elves=( "${4}" "${5}" )
  local result_file="${6}"

  if [[ ! -f "${result_file}" ]]; then
    echo "${result_file} not found. Run 'make BOARD=cr50' first" >&2
    exit 1
  fi

  # If signing a chip factory image (version 0.0.22) do not try figuring out the
  # RMA keys.
  local ignore_rma_keys="$(awk '
    BEGIN {count = 0};
    /"major": 0,/ {count += 1};
    /"minor": 22,/ {count += 1};
    END {{if (count == 2) {print "yes"};}}' \
	  "${manifest_file}")"

  # TODO(davidriley): pass rma keys
  ignore_rma_keys=yes

  if [ "${ignore_rma_keys}" != "yes" ]; then
    rma_key_base="$(determine_rma_key_base ${elves[@]})"
  else
    echo "Ignoring RMA keys for factory branch"
  fi

  local signer_command_params=()
  signer_command_params+=(--b -x "${fuses_file}")
  signer_command_params+=(-k "${key_file}")

  # Swap test public RMA server key with the prod version.
  if [ "${ignore_rma_keys}" != "yes" ]; then
  signer_command_params+=(-S "${rma_key_base}.test","${rma_key_base}.prod")
  fi
  signer_command_params+=(-j "${manifest_file}")

  signer_command_params+=(--format=bin)
  dst_suffix='flat'

  count=0
  for elf in ${elves[@]}; do
    if strings "${elf}" | grep -q "DBG/cr50"; then
      echo "Will not sign debug image with prod keys" >&2
      exit 1
    fi
    signed_file="${count}.${dst_suffix}"

    # Make sure output file is not owned by root
    touch "${signed_file}"
    NOW=$(date +%s)
    if ! cr50-codesigner ${signer_command_params[@]} \
	-i ${elf} -o "${signed_file}"; then
      error "cr50-codesigner failed"
      exit 1
    fi

    if [ "${ignore_rma_keys}" != "yes" ]; then
      if find_blob_in_blob  "${signed_file}" "${rma_key_base}.test"; then
	error "test RMA key in the signed image!"
	rm *."${dst_suffix}"
	exit 1
      fi

      if ! find_blob_in_blob "${signed_file}" "${rma_key_base}.prod"; then
	error "prod RMA key not in the signed image!"
	rm *."${dst_suffix}"
	exit 1
      fi
    fi
    : $(( count++ ))
  done

  # Full binary image is required, paste the newly signed blobs into the
  # output image.
  dd if="0.flat" of="${result_file}" seek=16384 bs=1 conv=notrunc
  dd if="1.flat" of="${result_file}" seek=278528 bs=1 conv=notrunc
  rm [01].flat

  echo "SUCCESS!!!"
}

# A very crude RO verification function. The key signature found at a fixed
# offset into the RO blob must match the RO type. Prod keys have bit D2 set to
# one, dev keys have this bit set to zero.
verify_ro() {
  local ro_bin="${1}"
  local key_byte

  if [ ! -f "${ro_bin}" ]; then
    error "${ro_bin} not a file!"
    exit 1
  fi

  # Key signature's lowest byte is byte #5 in the line at offset 0001a0.
  key_byte="$(od -Ax -t x1 -v "${ro_bin}" | awk '/0001a0/ {print $6};')"
  case "${key_byte}" in
    (?[4567cdef])
      return 0
      ;;
    (?[012389ab])
      ;;
  esac

  error "RO key in ${ro_bin} does not match type prod"
  exit 1
}

# This function prepares a full CR50 image, consisting of two ROs and two RWs
# placed at their respective offsets into the resulting blob. It invokes the
# bs (binary signer) script to actually convert elf versions of RWs into
# binaries and sign them.
#
# The signed image is placed in the directory named as concatenation of RO and
# RW version numbers and board ID fields, if set to non-default. The ebuild
# downloading the tarball from the BCS expects the image to be in that
# directory.
sign_cr50_firmware() {
  [[ $# -eq 8 ]] || \
    die "Usage: sign_cr50_firmware <key_file> <manifest> <fuses> " \
        "<ro_a> <ro_b> <rw_a> <rw_b> <output>"

  local key_file="${1}"
  local manifest_file="${2}"
  local fuses_file="${3}"
  local ro_a_hex="${4}"
  local ro_b_hex="${5}"
  local rw_a="${6}"
  local rw_b="${7}"
  local output_file="${8}"

  local temp_dir="$(make_temp_dir)"

  IMAGE_SIZE='524288'

  dd if=/dev/zero bs="${IMAGE_SIZE}" count=1  2>/dev/null |
    tr \\000 \\377 > "${output_file}"
  if [ "$(stat -c '%s' "${output_file}")" != "${IMAGE_SIZE}" ]; then
    error "Failed creating ${output_file}"
    exit 1
  fi

  local count=0

  for f in "${ro_a_hex}" "${ro_b_hex}"; do
    if ! objcopy -I ihex "${f}" -O binary "${temp_dir}/${count}.bin"; then
      error "Failed to convert ${f} from hex to bin"
      exit 1
    fi
    verify_ro "${temp_dir}/${count}.bin"
    : $(( count += 1 ))
  done

  if ! do_bs "${key_file}" "${manifest_file}" "${fuses_file}" \
    "${rw_a}" "${rw_b}" "${output_file}" > /dev/null;
  then
    error "Failed invoking do_bs elves ${rw_a} ${rw_b}"
    exit 1
  fi

  dd if="${temp_dir}/0.bin" of="${output_file}" conv=notrunc
  dd if="${temp_dir}/1.bin" of="${output_file}" seek=262144 bs=1 conv=notrunc

  echo "SUCCESS!!!!!!"
  echo "image copied to ${output_file}"
}

# Sign the directory holding cr50 firmware.
sign_cr50_firmware_dir() {
  [[ $# -eq 3 ]] || die "Usage: sign_cr50_firmware_dir <input> <key> <output>"
  local input="${1%/}"
  local key_file="$2"
  local output="$3"

  if [[ -d "${output}" ]]; then
    output="${output}/cr50.bin.prod"
  fi

  sign_cr50_firmware \
	  "${key_file}" \
	  "${input}/ec_RW-manifest-prod.json" \
	  "${input}/fuses.xml" \
	  "${input}/prod.ro.A" \
	  "${input}/prod.ro.B" \
	  "${input}/ec.RW.elf" \
	  "${input}/ec.RW_B.elf" \
	  "${output}"
}

main() {
  if [[ $# -ne 3 ]]; then
    flags_help
    exit 1
  fi

  local input="${1%/}"
  local key_dir="$2"
  local output="$3"

  local key_file="${key_dir}/cr50.pem"
  if [[ ! -e "${key_file}" ]]; then
    die "Missing key file: ${key_file}"
  fi

  if [[ ! -d "${input}" ]]; then
    die "Missing input directory: ${input}"
  fi

  sign_cr50_firmware_dir "${input}" "${key_file}" "${output}"
}
main "$@"
