#!/bin/bash

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# abort on error
set -e

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

required_bits_guybrush=$((1 << 58)) # PSP_S0I3_RESUME_VERSTAGE bit
forbidden_bits_guybrush="0x0"

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

  echo "${board}"

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

  local required_bits forbidden_bits required_name forbidden_name
  required_name=required_bits_${board}
  forbidden_name=forbidden_bits_${board}
  required_bits=${!required_name}
  forbidden_bits=${!forbidden_name}

  if [ -z "${forbidden_bits}" ] && [ -z "${required_bits}" ]; then
    # Don't do any checking if we don't have at least one set of bits defined
    sudo umount "${rootfs}"
    exit
  fi

  # Substitute 0 if a set is missing
  required_bits=${required_bits:-"0x0"}
  forbidden_bits=${forbidden_bits:-"0x0"}

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
    local soft_fuse forbidden_set
    soft_fuse=$(amdfwread --soft-fuse "${image}" | \
      sed -E -n 's/Soft-fuse:(0[xX][0-9a-fA-F]+)/\1/p')

    if [ -z "${soft_fuse}" ]; then
      die_umount "Failed to get soft-fuse bits from firmware" "${rootfs}"
    fi

    forbidden_set=$((soft_fuse & forbidden_bits))
    if [ "${forbidden_set}" != 0 ]; then
      die_umount "$(printf "%s: Forbidden AMD PSP soft fuse bits set: 0x%x" \
        "${image}" "${forbidden_set}")" "${rootfs}"
    fi

    if [ $((soft_fuse & required_bits)) != $((required_bits)) ]; then
      die_umount "$(printf "%s: Required AMD PSP soft fuse bits not set: 0x%x" \
        "${image}" "$((~soft_fuse & required_bits))")" "${rootfs}"
    fi
  done
}
main "$@"
