#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Setup the default key configuration by using the local key in `key_dir`.
setup_default_keycfg() {
  local key_dir=$1
  export KEYCFG_KERNEL_KEYBLOCK="${key_dir}/kernel.keyblock"
  export KEYCFG_KERNEL_VBPRIVK="${key_dir}/kernel_data_key.vbprivk"
  export KEYCFG_MINIOS_KERNEL_KEYBLOCK="${key_dir}/minios_kernel.keyblock"
  export KEYCFG_MINIOS_KERNEL_V1_KEYBLOCK="${key_dir}/minios_kernel.v1.keyblock"
  export KEYCFG_MINIOS_KERNEL_VBPRIVK="${key_dir}/minios_kernel_data_key.vbprivk"
  export KEYCFG_RECOVERY_KERNEL_KEYBLOCK="${key_dir}/recovery_kernel.keyblock"
  export KEYCFG_RECOVERY_KERNEL_V1_KEYBLOCK="${key_dir}/recovery_kernel.v1.keyblock"
  export KEYCFG_RECOVERY_KERNEL_VBPRIVK="${key_dir}/recovery_kernel_data_key.vbprivk"
  export KEYCFG_INSTALLER_KERNEL_KEYBLOCK="${key_dir}/installer_kernel.keyblock"
  export KEYCFG_INSTALLER_KERNEL_V1_KEYBLOCK="${key_dir}/installer_kernel.v1.keyblock"
  export KEYCFG_INSTALLER_KERNEL_VBPRIVK="${key_dir}/installer_kernel_data_key.vbprivk"
  export KEYCFG_ARV_PLATFORM_KEYBLOCK="${key_dir}/arv_platform.keyblock"
  export KEYCFG_ARV_PLATFORM_VBPRIVK="${key_dir}/arv_platform.vbprivk"
  export KEYCFG_KEY_EC_EFS_VBRPIK2="${key_dir}/key_ec_efs.vbprik2"
  export KEYCFG_ACCESSORY_RWSIG_VBRPIK2=""
  export KEYCFG_FIRMWARE_VBPRIVK="${key_dir}/firmware_data_key.vbprivk"
  declare -a KEYCFG_KEY_FIRMARE_VBPRIVK_LOEM
}

setup_keycfg() {
  local key_dir=$1
  setup_default_keycfg "${key_dir}"
  if [ -f "${key_dir}/key_config.sh" ]; then
    . "${key_dir}/key_config.sh"
  fi
}

get_firmware_loem_vbprivk() {
  local key_dir=$1
  local key_index=$2
  local default="${key_dir}/firmware_data_key.loem${key_index}.vbprivk"
  echo ${KEYCFG_KEY_FIRMARE_VBPRIVK_LOEM[$key_index]:-$default}
}
