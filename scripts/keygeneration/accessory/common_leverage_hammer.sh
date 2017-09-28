#!/bin/bash

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and functions.
. "$(dirname "${0}")/../common.sh"

usage() {
  cat <<EOF
Usage: "${PROG}" [options]

Options:
  -o, --output_dir <dir>:    Where to write the keys (default is cwd)
EOF

  if [[ $# -ne 0 ]]; then
    die "$*"
  else
    exit 0
  fi
}

leverage_hammer_to_create_key() {
  local output_dir="${PWD}"
  local key_name="${1}"
  shift

  while [[ $# -gt 0 ]]; do
    echo "$1" >&2
    case "${1}" in
    -h|--help)
      usage
      ;;
    -o|--output_dir)
      output_dir="${2}"
      if [[ ! -d "${output_dir}" ]]; then
        die "output dir ("${output_dir}") doesn't exist."
      fi
      shift
      ;;
    -*)
      usage "Unknown option: "${1}""
      ;;
    *)
      usage "Unknown argument "${1}""
      ;;
    esac
    shift
  done

  ./create_new_hammer_keys.sh --output_dir "${output_dir}" \
    --key_name "${key_name}"
}
