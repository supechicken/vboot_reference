# Copyright 2024 The ChromiumOS Authors

fwlib-y += \
	2lib/2api.c \
	2lib/2auxfw_sync.c \
	2lib/2common.c \
	2lib/2context.c \
	2lib/2crc8.c \
	2lib/2crypto.c \
	2lib/2ec_sync.c \
	2lib/2firmware.c \
	2lib/2gbb.c \
	2lib/2hmac.c \
	2lib/2kernel.c \
	2lib/2load_kernel.c \
	2lib/2misc.c \
	2lib/2nvstorage.c \
	2lib/2packed_key.c \
	2lib/2recovery_reasons.c \
	2lib/2rsa.c \
	2lib/2secdata_firmware.c \
	2lib/2secdata_fwmp.c \
	2lib/2secdata_kernel.c \
	2lib/2sha1.c \
	2lib/2sha256.c \
	2lib/2sha512.c \
	2lib/2sha_utility.c \
	2lib/2struct.c \
	2lib/2stub_hwcrypto.c \
	2lib/2tpm_bootmode.c \
	lib20/api_kernel.c \
	lib20/kernel.c \
	lib/cgptlib/cgptlib.c \
	lib/cgptlib/cgptlib_internal.c \
	lib/cgptlib/crc32.c \
	lib/gpt_misc.c

# Support real TPM unless MOCK_TPM is set
fwlib-$(if $(filter-out 0,${MOCK_TPM}),y) += lib/tpm_lite/mocked_tlcl.c

ifneq ($(filter-out 0,${X86_SHA_EXT}),)
fwlib-cflags += -DX86_SHA_EXT
fwlib-y += \
	2lib/2hwcrypto.c \
	2lib/2sha256_x86.c
endif

ifneq ($(filter-out 0,${ARMV8_CRYPTO_EXT}),)
fwlib-fclags += -DARMV8_CRYPTO_EXT
fwlib-y += \
	2lib/2hwcrypto.c \
	2lib/2sha256_arm.c \
	2lib/sha256_armv8a_ce_a64.S
endif

ifneq ($(filter-out 0,${ARM64_RSA_ACCELERATION}),)
fwlib-cflags += -DARM64_RSA_ACCELERATION
fwlib-y += 2lib/2modpow_neon.c
endif

ifneq ($(filter-out 0,${VB2_X86_RSA_ACCELERATION}),)
fwlib-cflags += -DVB2_X86_RSA_ACCELERATION
fwlib-y += 2lib/2modpow_sse2.c
endif

ifneq (,$(filter arm64 x86 x86_64,${ARCH}))
ENABLE_HWCRYPTO_RSA_TESTS := 1
endif

# Even if X86_SHA_EXT is 0 we need cflags since this will be compiled for tests
# TODO(czapiga): make it easier to add flags for specific files
${BUILD}/firmware/2lib/2sha256_x86.futil.o: CFLAGS += -mssse3 -mno-avx -msha

${BUILD}/firmware/2lib/2modpow_sse2.futil.o: CFLAGS += -msse2 -mno-avx

# Include BIOS stubs in the firmware library when compiling for host
# TODO: split out other stub funcs too
fwlib-$(if $(filter-out 0,${FIRMWARE_STUB}),y,) += \
	2lib/2stub.c \
	stub/tpm_lite_stub.c \
	stub/vboot_api_stub_disk.c \
	stub/vboot_api_stub_stream.c

hostlib-y += \
	2lib/2common.c \
	2lib/2context.c \
	2lib/2crc8.c \
	2lib/2crypto.c \
	2lib/2hmac.c \
	2lib/2nvstorage.c \
	2lib/2recovery_reasons.c \
	2lib/2rsa.c \
	2lib/2sha1.c \
	2lib/2sha256.c \
	2lib/2sha512.c \
	2lib/2sha_utility.c \
	2lib/2struct.c \
	2lib/2stub.c \
	2lib/2stub_hwcrypto.c \
	lib/cgptlib/cgptlib_internal.c \
	lib/cgptlib/crc32.c \
	lib/gpt_misc.c \
	stub/tpm_lite_stub.c \
	stub/vboot_api_stub_disk.c

# TPM lightweight command library
ifeq ($(filter-out 0,${TPM2_MODE}),)
tlcl-y += lib/tpm_lite/tlcl.c
else
# TODO(apronin): tests for TPM2 case?
tlcl-y += \
	lib/tpm2_lite/tlcl.c \
	lib/tpm2_lite/marshaling.c
endif

hostlib += $(tlcl-y)
