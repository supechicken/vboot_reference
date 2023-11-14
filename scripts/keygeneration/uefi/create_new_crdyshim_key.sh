#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and functions.
# shellcheck source=../common.sh
. "$(dirname "$0")/../common.sh"

usage() {
  cat <<EOF
Usage: ${PROG} [options]

Options:
  -o, --output <dir>:    Where to write the keys (default is cwd)
EOF

  if [[ $# -ne 0 ]]; then
    die "$*"
  else
    exit 0
  fi
}

generate_ed25519_key() {
  local output="$1"

  # Generate ed25519 private and public key.
  openssl genpkey -algorithm Ed25519 -out "${output}/crdyshim.priv.pem"
  openssl pkey -in "${output}/crdyshim.priv.pem" -pubout -text_pub \
    -out "${output}/crdyshim.pub.pem"
}

main() {
  set -euo pipefail

  local output="${PWD}"

  while [[ $# -gt 0 ]]; do
    case "$1" in
    -h|--help)
      usage
      ;;
    -o|--output)
      output="$2"
      if [[ ! -d "${output}" ]]; then
        die "output dir (${output}) doesn't exist."
      fi
      shift
      ;;
    -*)
      usage "Unknown option: $1"
      ;;
    *)
      usage "Unknown argument $1"
      ;;
    esac
    shift
  done

  generate_ed25519_key "${output}"
}

main "$@"
