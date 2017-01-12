#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

main() {
  set -e

  local image="$1"
  local pub_key="$2"
  if [ $# -ne 2 ]; then
    cat <<EOF
Usage: $PROG <image.bin> <public_key.pem>
Installs the container verification public key <public_key.pem> to <image.bin>.
EOF
    exit 1
  fi
  local rootfs=$(make_temp_dir)
  local key_location="/usr/share/misc/"
  mount_image_partition "${image}" 3 "${rootfs}"
  sudo install \
    -D -o root -g root -m 644 \
    "${pub_key}" "${rootfs}/${key_location}/oci-container-key.pem"
  info "Container verification key was installed." \
       "Do not forget to resign the image!"
}

main "$@"
