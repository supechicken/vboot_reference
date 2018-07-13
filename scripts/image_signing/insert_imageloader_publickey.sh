#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

load_shflags || exit 1

FLAGS_HELP="Usage: ${PROG} <image.bin|rootfs_dir> <public_key.pem> <image_type>

Installs the imageloader verification public key <public_key.pem> to
<image.bin|rootfs_dir>.

<image_type> is the type of the image that is being signed:
  * oci-container - The squashfs image that that provides an OCI container's
    rootfs and OCI configuration.
  * demo-mode-resources - The squashfs image that provides pre-installed
    resources to be used in demo mode sessions.
"

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error.
set -e

main() {
  if [[ $# -ne 3 ]]; then
    flags_help
    exit 1
  fi

  local image="$1"
  local pub_key="$2"
  local image_type="$3"
  local key_location="/usr/share/misc/"

  local target_key_name
  if [[ "${image_type}" == "oci-container" ]]; then
    target_key_name="oci-container-key-pub.der"
  elif [[ "${image_type}" == "demo-mode-resources" ]]; then
    target_key_name="demo-mode-resources-key-pub.der"
  else
    flags_help
    exit 1
  fi

  if [[ -d "${image}" ]]; then
    rootfs="${image}"
  else
    rootfs=$(make_temp_dir)
    mount_image_partition "${image}" 3 "${rootfs}"
  fi

  # Imageloader likes DER as a runtime format as it's easier to read.
  local tmpfile=$(make_temp_file)
  openssl pkey -pubin -in "${pub_key}" -out "${tmpfile}" -pubout -outform DER

  sudo install \
    -D -o root -g root -m 644 \
    "${tmpfile}" "${rootfs}/${key_location}/${target_key_name}"
  info "Container verification key was installed." \
       "Do not forget to resign the image!"
}

main "$@"
