#!/bin/bash

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and functions.
. "$(dirname "$0")/../common.sh"

usage() {
  cat <<EOF
Usage: ${PROG} [options]

Options:
  -o, --output_dir <dir>: Where to write the keys (default is cwd)
  -n, --key-name <name>:  Name of the key pair (default is hammer)
EOF

  if [[ $# -ne 0 ]]; then
    die "$*"
  else
    exit 0
  fi
}

# Generate a keypair at the given directory.
generate_key() {
  local output_dir="${1}"
  local key_name="${2}"
  local tmp_dir=$(mktemp -d --suffix=.create_${key_name}_keys)

  # Generate RSA key.
  openssl genrsa -3 -out "${tmp_dir}/temp.pem" 3072

  # Create a keypair from an RSA .pem file generated above.
  futility create "${tmp_dir}/temp.pem" "${output_dir}/key_${key_name}"

  # Best attempt to securely delete the temp.pem file.
  shred --remove "${tmp_dir}/temp.pem"
}

main() {
  set -e
  set -x
  local output_dir="${PWD}"
  local key_name="hammer"

  while [[ $# -gt 0 ]]; do
    case "${1}" in
    -h|--help)
      usage
      ;;
    -o|--output_dir)
      output_dir="${2}"
      if [[ ! -d "${output_dir}" ]]; then
        die "output dir (${output_dir}) doesn't exist."
      fi
      shift
      ;;
    -n|--key_name)
      key_name="${2}"
      shift
      ;;
    -*)
      usage "Unknown option: ${1}"
      ;;
    *)
      usage "Unknown argument ${1}"
      ;;
    esac
    shift
  done

  generate_key "${output_dir}" "${key_name}"
}

main "$@"
