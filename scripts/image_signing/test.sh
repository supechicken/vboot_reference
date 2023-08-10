#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

set -e

signer_tool="$HOME/chromiumos/src/platform/signing/signer-dev/signer/signingtools-bin"
#FUTILITY="${signer_tool}/futility"
FUTILITY="/usr/bin/futility"

fatal() {
  error "$@"
  exit -1
}

check_argc() {
  local actual=$1
  local expected=$2
  local func_name=${FUNCNAME[1]}
  test "$1" -eq "$2" && return 0
  error "${func_name}: Unexpected number of argument: expected ${expected}, got ${actual}"
  return -1
}

has_file() {
  check_argc $# 1
  local file_name=$1
  local func_name=${FUNCNAME[1]}
  test -f "$1" && return 0
  error "${func_name}: file '$file_name' doesn't exist"
  return -1
}

has_dir() {
  check_argc $# 1
  local dir_name=$1
  local func_name=${FUNCNAME[1]}
  test -d "$1" && return 0
  error "${func_name}: dir '$dir_name' doesn't exist"
  return -1
}

ensure() {
  Do "$@" || exit -1
}

arg_string() {
  for arg in "$@"; do
    case "$arg" in
      *\ *)
        printf -- "'$arg' "
        ;;
      *)
        printf -- "$arg "
        ;;
    esac
  done
}
Do() {
  local message=$(arg_string "$@")
  printf -- '$ %s\n' "$message" >&2
  "$@"
}

test_show() {(
  check_argc $# 1 || return -1
  local in_image=$1
  local key_dir=$2
  has_file $in_image || return -1
  local loopdev=$(loopback_partscan "${in_image}")
  local dm_partno=2
  local loop_kern="${loopdev}p2"

  ensure sudo -E \
    "${FUTILITY}" show --type help # "${loop_kern}"
)}

test_vbutil_kernel() {(
  check_argc $# 2 || return -1
  local in_image=$1
  local key_dir=$2
  has_file $in_image || return -1
  has_dir $key_dir || return -1
  local loopdev=$(loopback_partscan "${in_image}")
  local dm_partno=2
  local loop_kern="${loopdev}p2"
  local kern_keyblock="${key_dir}/kernel.keyblock"
  local kern_privkey="${key_dir}/kernel_data_key.vbprivk"
  #local p11_privkey="rsa_sign_raw_pkcs1_4096"
  local p11_lib="$HOME/Documents/pkcs11/libkmsp11-1.2-linux-amd64/libkmsp11.so"
  local p11_privkey="pkcs11:${p11_lib}:0:rsa_sign_raw_pkcs1_4096"
  local kern_version=1
  local out_vb="/tmp/out_vb"

  export KMS_PKCS11_CONFIG="${key_dir}/pkcs11-config.yaml"
  #export PKCS11_MODULE_PATH="$HOME/Documents/pkcs11/libkmsp11-1.2-linux-amd64/libkmsp11.so"

	local signpriv="${p11_privkey}"

  ensure sudo -E \
    "${FUTILITY}" --debug vbutil_kernel --repack "${out_vb}" \
    --keyblock "${kern_keyblock}" \
    --signprivate "${signpriv}" \
    --version "${kern_version}" \
    --oldblob "${loop_kern}"
    #--signprivate "${kern_privkey}" \
)}

test_sign_official_build() {(
  check_argc $# 3 || return -1
  local in_image=$1
  local key_dir=$2
  local out_image=$3
  has_file $in_image || return -1
  has_dir $key_dir || return -1
  export KMS_PKCS11_CONFIG="${key_dir}/pkcs11-config.yaml"
  local sign_official_build="$HOME/chromiumos/src/platform/vboot_reference/scripts/image_signing/sign_official_build.sh"
  local out_image="/tmp/out_image.bin"
  "${sign_official_build}" base "${in_image}" "${key_dir}" "${out_image}"
  #"${sign_official_build}" accessory_rwsig "${in_image}" "${key_dir}" "${out_image}"
)}

main() {
  check_argc $# 3 || return -1
  local in_image=$1
  local key_dir=$2
  local out_image=$3
  has_file $in_image || return -1
  has_dir $key_dir || return -1

  #Do test_show "$@" || exit -1
  #info "Testing futility vbutil_kernel"
  #Do test_vbutil_kernel "${in_image}" "${key_dir}" || exit -1
  info "Testing sign_official_build.sh"
  Do test_sign_official_build "${in_image}" "${key_dir}" "${out_image}" || exit -1
}

main "$@"
