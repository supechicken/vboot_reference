#!/bin/bash
echo "sourcing key_config.sh"
#P11_LIB="$HOME/Documents/pkcs11/libkmsp11-1.2-linux-amd64/libkmsp11.so"
P11_LIB="$HOME/Documents/pkcs11/libkmsp11.so"
KEYCFG_KERNEL_KEYBLOCK="${KEY_DIR}/kms_rsa_key2.keyblock"
KEYCFG_KERNEL_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_MINIOS_KERNEL_KEYBLOCK="${KEY_DIR}/kms_rsa_key2.keyblock"
KEYCFG_MINIOS_KERNEL_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_RECOVERY_KERNEL_KEYBLOCK="${KEY_DIR}/kms_rsa_key2.keyblock"
KEYCFG_RECOVERY_KERNEL_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_INSTALLER_KERNEL_KEYBLOCK="${KEY_DIR}/kms_rsa_key2.keyblock"
KEYCFG_INSTALLER_KERNEL_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_ARV_PLATFORM_KEYBLOCK="${KEY_DIR}/kms_rsa_key2.keyblock"
KEYCFG_ARV_PLATFORM_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_FIRMWARE_VBPRIVK="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_ACCESSORY_RWSIG_VBRPIK2="remote:${P11_LIB}:0:rsa_key2"
KEYCFG_UPDATE_KEY_PEM="remote:${P11_LIB}:0:rsa_key2"

KEYCFG_KEY_EC_EFS_VBRPIK2="remote:${P11_LIB}:0:rsa_key2"

for i in {1..4};
do
  KEYCFG_FIRMARE_VBPRIVK_LOEM[${i}]="remote:${P11_LIB}:0:rsa_key2"
done
