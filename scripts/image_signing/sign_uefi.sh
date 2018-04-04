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

sign_efi_file() {
  local target=$1
  local temp_dir=$2
  local priv_key=$3
  local sign_cert=$4
  local verify_cert=$5
  if [[ -z ${verify_cert} ]]; then
    verify_cert="${sign_cert}"
  fi

  info "Signing efi file ${target}"
  local signed_file="${temp_dir}/$(basename ${target})"
  sbsign --key="${priv_key}" --cert="${sign_cert}" \
      --output="${signed_file}" "${target}" || warn "Cannot sign ${target}"
  if [[ -f ${signed_file} ]]; then
    sudo cp -f "${signed_file}" "${target}"
    sbverify --cert "${verify_cert}" "${target}" || die "Verification failed"
  fi
}

install_gsetup_cert() {
  local key_type=$1
  local cert=$2
  local gsetup_dir=$3
  if [[ -f $cert ]]; then
    info "Putting $key_type cert: ${cert}"
    sudo mkdir -p "${gsetup_dir}/${key_type}"
    sudo cp $cert "${gsetup_dir}/${key_type}"
  else
    info "No $key_type cert: ${cert}"
  fi
}

main() {
  local esp_dir=$1
  local key_dir=$2

  if [[ $# -ne 2 ]]; then
    usage "command takes exactly 2 args"
  fi

  if ! type -P sbsign &>/dev/null; then
    warn "Skip signing UEFI binaries (sbsign not found)."
    exit 1
  fi
  if ! type -P sbverify &>/dev/null; then
    warn "Skip signing UEFI binaries (sbverify not found)."
    exit 1
  fi

  local bootloader_dir="${esp_dir}/efi/boot"
  local kernel_dir="${esp_dir}/syslinux"
  local gsetup_dir="${esp_dir}/EFI/Google/GSetup"

  local pk_dir="${key_dir}/pk"
  if [[ ! -d $pk_dir ]]; then
    warn "No pk directory: ${pk_dir}"
    exit 1
  fi
  local pk_cert_der=$(find_latest_file ${pk_dir} pk .der)
  install_gsetup_cert pk "${pk_cert_der}" "${gsetup_dir}"

  local db_dir="${key_dir}/db"
  if [[ ! -d $db_dir ]]; then
    warn "No db directory: ${db_dir}"
    exit 1
  fi
  local db_cert=$(find_latest_file ${db_dir} db .pem)
  if [[ ! -f $db_cert ]]; then
    warn "Invalid verification cert: ${db_cert}"
    exit 1
  fi
  local db_cert_der=$(replace_file_extension ${db_cert} der)
  install_gsetup_cert db "${db_cert_der}" "${gsetup_dir}"

  local child_key_dir=$(replace_file_extension ${db_cert} children)
  if [[ ! -d $child_key_dir ]]; then
    warn "Invalid child key directory: ${child_key_dir}"
    exit 1
  fi
  local signing_key=$(find_latest_file ${child_key_dir} child .rsa)
  if [[ ! -f $signing_key ]]; then
    warn "Invalid signing key: ${signing_key}"
    exit 1
  fi
  local signing_cert=$(replace_file_extension ${signing_key} pem)
  if [[ ! -f $signing_cert ]]; then
    warn "Invalid signing cert: ${signing_cert}"
    exit 1
  fi
  info "Signing key: ${signing_key}"
  info "Signing cert: ${signing_cert}"
  info "Verification cert: ${db_cert}"

  local working_dir=$(make_temp_dir)

  for efi_file in "${bootloader_dir}/"*".efi"; do
    if [[ ! -f "${efi_file}" ]]; then
      continue
    fi
    sign_efi_file "${efi_file}" "${working_dir}" \
        "${signing_key}" "${signing_cert}" "${db_cert}"
  done

  for kernel_file in "${kernel_dir}/vmlinuz."?; do
    if [[ ! -f "${kernel_file}" ]]; then
      continue
    fi
    sign_efi_file "${kernel_file}" "${working_dir}" \
        "${signing_key}" "${signing_cert}" "${db_cert}"
  done

  local kek_dir="${key_dir}/kek"
  local kek_cert_der=$(find_latest_file ${kek_dir} kek .der)
  install_gsetup_cert kek "${kek_cert_der}" "${gsetup_dir}"

  loca dbx_dir="${key_dir}/dbx"
  for dbx_cert_der in "${dbx_dir}/"*".der"; do
    install_gsetup_cert dbx "${dbx_cert_der}" "${gsetup_dir}"
  done
}

main "$@"
