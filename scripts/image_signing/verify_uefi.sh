#!/bin/bash

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

set -e

usage() {
  cat <<EOF
Usage: $PROG /path/to/esp/dir

Verify signatures of UEFI binaries in ESP.
EOF
  if [[ $# -gt 0 ]]; then
    error "$*"
    exit 1
  fi
  exit 0
}

main() {
  local esp_dir=$1

  if [[ $# -ne 1 ]]; then
    usage "command takes exactly 1 args"
  fi

  if ! type -P sbverify &>/dev/null; then
    die "Cannot verify UEFI signatures (sbverify not found)."
  fi

  local bootloader_dir="${esp_dir}/efi/boot"
  local kernel_dir="${esp_dir}/syslinux"
  local gsetup_dir="${esp_dir}/EFI/Google/GSetup"

  local pk_dir="${gsetup_dir}/pk"
  local pk_cert_der=$(find_latest_file ${pk_dir} pk .der)
  if [[ ! -f $pk_cert_der ]]; then
    warn "Invalid pk cert: ${pk_cert_der}"
    exit 0
  fi

  local db_dir="${gsetup_dir}/db"
  local db_cert_der=$(find_latest_file ${db_dir} db .der)
  if [[ ! -f $db_cert_der ]]; then
    die "Invalid verification cert: ${db_cert_der}"
  fi
  info "Verification cert: ${db_cert_der}"

  local working_dir=$(make_temp_dir)
  local cert="${working_dir}/cert.pem"
  openssl x509 -in "${db_cert_der}" -inform DER -out "${cert}" -outform PEM

  for efi_file in "${bootloader_dir}/"*".efi"; do
    if [[ ! -f "${efi_file}" ]]; then
      continue
    fi
    sbverify --cert "${cert}" "${efi_file}" ||
        die "Verification failed: ${efi_file}"
  done

  for kernel_file in "${kernel_dir}/vmlinuz."?; do
    if [[ ! -f "${kernel_file}" ]]; then
      continue
    fi
    sbverify --cert "${cert}" "${kernel_file}" ||
        warn "Verification failed: ${kernel_file}"
  done
}

main "$@"
