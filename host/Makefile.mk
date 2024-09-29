# Copyright 2024 The ChromiumOS Authors


futil-$(if $(filter-out 0,${USE_FLASHROM}),y) += lib/flashrom_drv.c

ifneq ($(filter-out 0,${USE_FLASHROM}),)
hostlib-y += \
	lib/flashrom.c \
	lib/flashrom_drv.c
CFLAGS += -DUSE_FLASHROM
endif

hostlib-y += \
	arch/${ARCH_DIR}/lib/crossystem_arch.c \
	lib21/host_misc.c \
	lib/cbfstool.c \
	lib/chromeos_config.c \
	lib/crossystem.c \
	lib/crypto.c \
	lib/extract_vmlinuz.c \
	lib/fmap.c \
	lib/host_misc.c \
	lib/subprocess.c

ifneq ($(filter-out 0,${USE_FLASHROM}),)
utillib-y += \
	lib/flashrom.c \
	lib/flashrom_drv.c
endif

utillib-y = \
	arch/${ARCH_DIR}/lib/crossystem_arch.c \
	lib21/host_common.c \
	lib21/host_key.c \
	lib21/host_misc.c \
	lib21/host_signature.c \
	lib/cbfstool.c \
	lib/chromeos_config.c \
	lib/crossystem.c \
	lib/crypto.c \
	lib/file_keys.c \
	lib/fmap.c \
	lib/host_common.c \
	lib/host_key2.c \
	lib/host_keyblock.c \
	lib/host_misc.c \
	lib/host_signature2.c \
	lib/host_signature.c \
	lib/signature_digest.c \
	lib/subprocess.c \
	lib/util_misc.c

ifeq ($(HAVE_NSS),1)
utillib-y += \
	lib/host_p11.c
else
utillib-y += \
	lib/host_p11_stub.c
endif

