#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

load_shflags || exit 1

DEFINE_string output "" \
  "Where to write signed output to (default: sign in-place)"

FLAGS_HELP="Usage: ${PROG} [options] <input_image> <key_dir>

Signs <input_image> with keys in <key_dir>. Should have an imageloader.json
file which imageloader can understand and will use to mount the squashfs
image that provides the container's rootfs and OCI configuration.

Input can be an unpacked imageloader image, or a CRX/ZIP file.
"

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error.
set -e

main() {
  if [[ $# -ne 2 ]]; then
    flags_help
    exit 1
  fi

  # TODO(tbarzic): Replace usages of sign_oci_container.sh with
  # sign_imageloader_image.sh, and then remove sign_oci_container.sh.
  "${SCRIPT_DIR}/sign_imageloader_image.sh" \
      "$1" "$2" "oci-container" --output "${output}"
}
main "$@"
