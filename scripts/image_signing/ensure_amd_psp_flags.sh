#!/bin/bash

# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# abort on error
set -e

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

required_bit_mask_guybrush=$((1 << 58)) # PSP_S0I3_RESUME_VERSTAGE bit
forbidden_bit_mask_guybrush="0x0"

required_bit_mask_zork="0x0"
forbidden_bit_mask_zork="0x0"

# Grunt uses an old firmware format that amdfwread cannot read
# See b/233787191 for skyrim
boards_ignore_list="grunt skyrim"

die_umount() {
  sudo umount "$2"
  die "$1"
}

main() {
if [[ $# -ne 2 ]]; then
    echo "Usage $0 <board> <image>"
    exit 1
  fi

  local board="$1"
  local image="$2"

  for ignored in ${boards_ignore_list}
  do
    if [ "${board}" == "${ignored}" ]; then
      exit 0
    fi
  done

  local loopdev rootfs
  if [[ -d "${image}" ]]; then
    rootfs="${image}"
  else
    rootfs=$(make_temp_dir)
    loopdev=$(loopback_partscan "${image}")
    mount_loop_image_partition_ro "${loopdev}" 3 "${rootfs}"
  fi

  # Get the board for this image
  local firmware_bundle shellball_dir
  firmware_bundle="${rootfs}/usr/sbin/chromeos-firmwareupdate"
  shellball_dir=$(make_temp_dir)

  local required_bit_mask forbidden_bit_mask required_name forbidden_name
  required_name=required_bit_mask_${board}
  forbidden_name=forbidden_bit_mask_${board}
  required_bit_mask=${!required_name}
  forbidden_bit_mask=${!forbidden_name}

  if [ -z "${forbidden_bit_mask}" ] || [ -z "${required_bit_mask}" ]; then
    # Don't do any checking if we don't have at least one set of bits defined
    die_umount "${rootfs}" "No PSP check bit masks set for board ${board}"
  fi

  # Substitute 0 if a set is missing
  required_bit_mask=${required_bit_mask:-"0x0"}
  forbidden_bit_mask=${forbidden_bit_mask:-"0x0"}

  # Extract our firmware
  if ! extract_firmware_bundle "${firmware_bundle}" "${shellball_dir}"; then
    die_umount "${rootfs}" "Failed to extract firmware bundle"
  fi

  # Check the soft fuse bits in each FW image
  local images
  images=$(ls "${shellball_dir}"/images/bios-*)
  for image in ${images}
  do
    # Output is Soft-fuse:value
    local soft_fuse forbidden_set missing_set
    soft_fuse=$(amdfwread --soft-fuse "${image}" | \
      sed -E -n 's/Soft-fuse:(0[xX][0-9a-fA-F]+)/\1/p')

    if [ -z "${soft_fuse}" ]; then
      die_umount "Failed to get soft-fuse bits from firmware" "${rootfs}"
    fi

    forbidden_set=$((soft_fuse & forbidden_bit_mask))
    if [ "${forbidden_set}" != 0 ]; then
      die_umount "$(printf "%s: Forbidden AMD PSP soft fuse bits set: 0x%x" \
        "${image}" "${forbidden_set}")" "${rootfs}"
    fi

    missing_set=$((~soft_fuse & required_bit_mask))
    if [ "${missing_set}" != 0 ]; then
      die_umount "$(printf "%s: Required AMD PSP soft fuse bits not set: 0x%x" \
        "${image}" "${missing_set}")" "${rootfs}"
    fi
  done
}
main "$@"
