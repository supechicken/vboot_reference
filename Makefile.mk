# Copyright 2024 The ChromiumOS Authors

classes-y += cgpt cgpt_wrapper
cgpt-type := executable
cgpt_wrapper-type := executable

classes-y += futil
futil-type := executable

classes-y += fwlib
fwlib-type := static-lib

classes-y += hostlib
hostlib-type := static-lib shared-lib

classes-y += tlcl
tlcl-type := static-lib

classes-y += utillib
utillib-type := static-lib

# TODO: Fix fuzzers and coverage
# Add fuzzers only for x86_64 arch and only for clang compiler
classes-$(if $(filter x86_64,${ARCH}),$(if $(filter clang,${CC}),y)) += fuzzers
# Add coverage support for coverage if enabled
classes-$(if $(filter-out 0,${COV}),y) += coverage

# SDK utilities
classes-y += \
	dumpRSAPublicKey \
	load_kernel_test \
	pad_digest_utility \
	signature_digest_utility \
	verify_data

# Device utilities
classes-y += \
	dumpRSAPublicKey \
	tpmc

ifneq ($(filter-out 0,${USE_FLASHROM}),)
classes-y += crossystem
endif

subdirs-y += cgpt
subdirs-y += firmware
subdirs-y += futility
subdirs-y += host
subdirs-y += utility

INCLUDES += \
	-Ifirmware/include \
	-Ifirmware/lib/include \
	-Ifirmware/lib/cgptlib/include \
	-Ifirmware/lib/tpm_lite/include \
	-Ifirmware/2lib/include

# If we're not building for a specific target, just stub out things like the
# TPM commands and various external functions that are provided by the BIOS.
ifneq (${FIRMWARE_STUB},)
INCLUDES += -Ihost/include -Ihost/lib/include
INCLUDES += -Ihost/lib21/include
ifeq ($(shell uname -s), OpenBSD)
INCLUDES += -I/usr/local/include
endif
endif

CFLAGS += $(INCLUDES)

