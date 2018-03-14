#!/bin/bash

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and functions.
. "$(dirname "$0")/common.sh"

usage() {
  cat <<EOF
Usage: ${PROG} DIR BOARD_NAME

Generate key pairs for UEFI secure boot.
EOF

  if [[ $# -ne 0 ]]; then
    die "$*"
  else
    exit 0
  fi
}

CHROMIUM_OS_SUBJECT='/C=US/ST=California/L=Mountain View/O=Google LLC.' \
    '/OU=Chromium OS'

# Get an available new file name.
get_new_filename() {
  local prefix=$1
  local filename="${prefix}-$(date +%Y%m%d-%H%M%S)"
  while [[ -f ${filename}.rsa || -f ${filename}.pem || -f ${filename}.csr || \
           -f ${filename}.der ]]; do
    sleep 1
    filename="${filename_prefix}-$(date +%Y%m%d-%H%M%S)"
  done
  echo ${filename}
}

# Generate a pair of a private key and a self-signed cert at the given
# directory.
# Return the base filename without extension.
make_self_signed_pair() {
  local dir=$1
  local filename_prefix=$2
  local subj=$3

  mkdir -p "${dir}"
  pushd "${dir}" > /dev/null
  local filename="$(get_new_filename ${filename_prefix})"
  openssl req -new -x509 -nodes -newkey rsa:2048 -sha256 \
      -keyout "${filename}.rsa" -out "${filename}.pem" -subj "${subj}" \
      -days 73000
  openssl x509 -in "${filename}.pem" -inform PEM \
      -out "${filename}.der" -outform DER
  echo "${filename}"
  popd > /dev/null
}

# Generate a pair of a private key and a cert signed by the given CA in the
# given directory.
# The CA file name is the base name without extension.
# The results are generated in "$2.children" directory under $1.
make_child_pair() {
  local dir=$1
  local ca_filename=$2  # Base filename without extension.
  local filename_prefix=$3
  local subj=$4

  pushd "${dir}" > /dev/null
  mkdir "${ca_filename}.children"
  cd "${ca_filename}.children"
  local filename="$(get_new_filename ${filename_prefix})"
  openssl req -new -nodes -newkey rsa:2048 -sha256 \
      -keyout "${filename}.rsa" -out "${filename}.csr" -subj "${subj}" \
      -days 73000
  openssl x509 -req -sha256 \
      -CA "../${ca_filename}.pem" -CAkey "../${ca_filename}.rsa" \
      -CAcreateserial -in "${filename}.csr" -out "${filename}.pem" -days 73000
  openssl x509 -in "${filename}.pem" -inform PEM \
      -out "${filename}.der" -outform DER
  popd > /dev/null
}

main() {
  set -e

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
    usage "Missing output directory and/or board name"
  fi

  local dir=$1
  local board_name=$2

  pushd "${dir}" > /dev/null
  if [[ ! -z ${board_name} ]]; then
    board_name=${board_name}" "
  fi
  local pk_key_filename=$(make_self_signed_pair pk pk \
      "${CHROMIUM_OS_SUBJECT}/CN=${board_name}UEFI Platform Key")
  local db_key_filename=$(make_self_signed_pair db db \
      "${CHROMIUM_OS_SUBJECT}/CN=${board_name}UEFI DB Key")
  make_child_pair db ${db_key_filename} child \
      "${CHROMIUM_OS_SUBJECT}/CN=${board_name}UEFI Child Key"
  popd > /dev/null
}

main "$@"
