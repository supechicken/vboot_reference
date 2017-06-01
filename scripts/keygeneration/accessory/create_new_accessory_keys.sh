#!/bin/bash

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

usage() {
  cat <<EOF
Usage: $0 DIR NAME

DIR: To generate the keypair at DIR
NAME: The keypair name would be key_NAME.*

EOF

  if [[ $# -ne 0 ]]; then
    echo "ERROR: $*" >&2
    exit 1
  else
    exit 0
  fi
}
# Generate a keypair at the given directory.
generate_key() {
  local dir=$1
  local name=$2

  # Generate RSA key.
  openssl genrsa -3 -out "${dir}/temp.pem" 3072

  # Create a keypair from an RSA .pem file generated above.
  futility create "${dir}/temp.pem" "${dir}/key_${name}"

  # Best attempt to securely delete the temp.pem file.
  shred --remove "${dir}/temp.pem"
}

main() {
  local dir
  local name

  while [[ $# -gt 0 ]]; do
    case $1 in
    -h|--help)
      usage
      ;;
    -*)
      usage "Unknown option: $1"
      ;;
    *)
      break
      ;;
    esac
  done

  if [[ $# -ne 2 ]]; then
    usage "Invalid argument."
  fi
  dir="$1"
  name="$2"

  generate_key "${dir}" "${name}"
}

main "$@"
