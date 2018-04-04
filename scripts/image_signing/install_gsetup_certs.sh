#!/bin/bash

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

set -e

usage() {
  cat <<EOF
Usage: $PROG /path/to/esp/dir /path/to/keys/dir

Sign UEFI binaries in ESP.
EOF
  if [[ $# -gt 0 ]]; then
    error "$*"
    exit 1
  fi
  exit 0
}

install_gsetup_cert() {
  local key_type="$1"
  local cert="$2"
  local gsetup_dir="$3"
  if [[ -f "$cert" ]]; then
    info "Putting $key_type cert: ${cert}"
    sudo mkdir -p "${gsetup_dir}/${key_type}"
    sudo cp "${cert}" "${gsetup_dir}/${key_type}"
  else
    info "No $key_type cert: ${cert}"
  fi
}

main() {
  local esp_dir="$1"
  local key_dir="$2"

  if [[ $# -ne 2 ]]; then
    usage "command takes exactly 2 args"
  fi

  local gsetup_dir="${esp_dir}/EFI/Google/GSetup"

  local pk_cert="${key_dir}/pk/pk.der"
  if [[ ! -f "$pk_cert" ]]; then
    die "No PK cert: ${pk_cert}"
  fi
  install_gsetup_cert pk "${pk_cert}" "${gsetup_dir}"

  local db_cert="${key_dir}/db/db.der"
  if [[ ! -f "$db_cert" ]]; then
    die "No DB cert: ${db_cert}"
  fi
  install_gsetup_cert db "${db_cert}" "${gsetup_dir}"

  local kek_cert="${key_dir}/kek/kek.der"
  install_gsetup_cert kek "${kek_cert}" "${gsetup_dir}"

  for dbx_cert in "${key_dir}/dbx/"*".der"; do
    install_gsetup_cert dbx "${dbx_cert}" "${gsetup_dir}"
  done
}

main "$@"
