# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifneq ($(V),1)
Q := @
endif

# This Makefile normally builds in a 'build' subdir, but use
#
#    make BUILD=<dir>
#
# to put the output somewhere else
BUILD ?= $(shell pwd)/build

# Target for 'make install'
DESTDIR ?= /usr/bin

#
# Provide default CC and CFLAGS for firmware builds; if you have any -D flags,
# please add them after this point (e.g., -DVBOOT_DEBUG).
#
# TODO(crosbug.com/16808) We hard-code u-boot's compiler flags here just
# temporarily. As we are still investigating which flags are necessary for
# maintaining a compatible ABI, etc. between u-boot and vboot_reference.
#
# As a first step, this makes the setting of CC and CFLAGS here optional, to
# permit a calling script or Makefile to set these.
#
# Flag ordering: arch, then -f, then -m, then -W
DEBUG_FLAGS := $(if ${DEBUG},-g -O0,-Os)
COMMON_FLAGS := -nostdinc -pipe \
	-ffreestanding -fno-builtin -fno-stack-protector \
	-Werror -Wall -Wstrict-prototypes $(DEBUG_FLAGS)

ifeq ($(FIRMWARE_ARCH), arm)
CC ?= armv7a-cros-linux-gnueabi-gcc
CFLAGS ?= -march=armv5 \
	-fno-common -ffixed-r8 \
	-mfloat-abi=hard -marm -mabi=aapcs-linux -mno-thumb-interwork \
	$(COMMON_FLAGS)
endif
ifeq ($(FIRMWARE_ARCH), i386)
CC ?= i686-pc-linux-gnu-gcc
# Drop -march=i386 to permit use of SSE instructions
CFLAGS ?= \
	-ffunction-sections -fvisibility=hidden -fno-strict-aliasing \
	-fomit-frame-pointer -fno-toplevel-reorder -fno-dwarf2-cfi-asm \
	-mpreferred-stack-boundary=2 -mregparm=3 \
	$(COMMON_FLAGS)
endif
ifeq ($(FIRMWARE_ARCH), x86_64)
CFLAGS ?= $(COMMON_FLAGS) \
	-fvisibility=hidden -fno-strict-aliasing -fomit-frame-pointer
endif

# Fix compiling directly on host (outside of emake)
ifeq ($(ARCH),)
ARCH = amd64
endif

CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config

ifeq ($(FIRMWARE_ARCH),)
CFLAGS += -DCHROMEOS_ENVIRONMENT -Wall -Werror
endif

ifneq (${DEBUG},)
CFLAGS += -DVBOOT_DEBUG
endif

ifeq (${DISABLE_NDEBUG},)
CFLAGS += -DNDEBUG
endif

# Create / use dependency files
CFLAGS += -MMD -MF $@.d

INCLUDES += \
	-Ifirmware/include \
	-Ifirmware/lib/include \
	-Ifirmware/lib/cgptlib/include \
	-Ifirmware/lib/cryptolib/include \
	-Ifirmware/lib/tpm_lite/include

ifeq ($(FIRMWARE_ARCH),)
INCLUDES += -Ifirmware/stub/include
else
INCLUDES += -Ifirmware/arch/$(FIRMWARE_ARCH)/include
endif

DUMPKERNELCONFIGLIB := ${BUILD}/libdump_kernel_config.a
FWLIB := ${BUILD}/vboot_fw.a
HOSTLIB := ${BUILD}/vboot_host.a
TEST_LIB := ${BUILD}/tests/test.a

CRYPTO_LIBS := $(shell $(PKG_CONFIG) --libs libcrypto)

PC_BASE_VER ?= 125070
PC_DEPS = libchrome-$(PC_BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LDLIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

# Create output directories if necessary.  Do this via explicit shell commands
# so it happens before trying to generate/include dependencies.
SUBDIRS := firmware host utility cgpt tests tests/tpm_lite
_dir_create := $(foreach d, \
	$(shell find $(SUBDIRS) -name '*.c' -exec  dirname {} \; | sort -u), \
	$(shell [ -d $(BUILD)/$(d) ] || mkdir -p $(BUILD)/$(d)))

# First target
all: fwlib $(if $(FIRMWARE_ARCH),,host_stuff)

# Host targets
host_stuff: fwlib hostlib cgpt utils tests

clean:
	$(Q)/bin/rm -rf ${BUILD}

install: cgpt_install utils_install

# -----------------------------------------------------------------------------
# Firmware library

# TPM-specific flags.  These depend on the particular TPM we're targeting for.
# They are needed here only for compiling parts of the firmware code into
# user-level tests.

# TPM_BLOCKING_CONTINUESELFTEST is defined if TPM_ContinueSelfTest blocks until
# the self test has completed.

$(FWLIB) : CFLAGS += -DTPM_BLOCKING_CONTINUESELFTEST

# TPM_MANUAL_SELFTEST is defined if the self test must be started manually
# (with a call to TPM_ContinueSelfTest) instead of starting automatically at
# power on.
#
# We sincerely hope that TPM_BLOCKING_CONTINUESELFTEST and TPM_MANUAL_SELFTEST
# are not both defined at the same time.  (See comment in code.)

# CFLAGS += -DTPM_MANUAL_SELFTEST

ifeq ($(FIRMWARE_ARCH),i386)
# Unrolling loops in cryptolib makes it faster
$(FWLIB) : CFLAGS += -DUNROLL_LOOPS

# Workaround for coreboot on x86, which will power off asynchronously
# without giving us a chance to react. This is not an example of the Right
# Way to do things. See chrome-os-partner:7689, and the commit message
# that made this change.
$(FWLIB) : CFLAGS += -DSAVE_LOCALE_IMMEDIATELY

# On x86 we don't actually read the GBB data into RAM until it is needed.
# Therefore it makes sense to cache it rather than reading it each time.
# Enable this feature.
$(FWLIB) : CFLAGS += -DCOPY_BMP_DATA
endif

ifeq ($(FIRMWARE_ARCH),)
$(warning FIRMWARE_ARCH not defined; assuming local compile)

# Disable rollback TPM when compiling locally, since otherwise
# load_kernel_test attempts to talk to the TPM.
$(FWLIB) : CFLAGS += -DDISABLE_ROLLBACK_TPM
endif

# find lib -iname '*.c' | sort
FWLIB_SRCS = \
	firmware/lib/cgptlib/cgptlib.c \
	firmware/lib/cgptlib/cgptlib_internal.c \
	firmware/lib/cgptlib/crc32.c \
	firmware/lib/crc8.c \
	firmware/lib/cryptolib/padding.c \
	firmware/lib/cryptolib/rsa.c \
	firmware/lib/cryptolib/rsa_utility.c \
	firmware/lib/cryptolib/sha1.c \
	firmware/lib/cryptolib/sha256.c \
	firmware/lib/cryptolib/sha512.c \
	firmware/lib/cryptolib/sha_utility.c \
	firmware/lib/stateful_util.c \
	firmware/lib/utility.c \
	firmware/lib/utility_string.c \
	firmware/lib/vboot_api_init.c \
	firmware/lib/vboot_api_firmware.c \
	firmware/lib/vboot_api_kernel.c \
	firmware/lib/vboot_audio.c \
	firmware/lib/vboot_common.c \
	firmware/lib/vboot_display.c \
	firmware/lib/vboot_firmware.c \
	firmware/lib/vboot_kernel.c \
	firmware/lib/vboot_nvstorage.c

ifeq ($(MOCK_TPM),)
FWLIB_SRCS += \
	firmware/lib/rollback_index.c \
	firmware/lib/tpm_bootmode.c \
	firmware/lib/tpm_lite/tlcl.c
else
FWLIB_SRCS += \
	firmware/lib/mocked_rollback_index.c \
	firmware/lib/mocked_tpm_bootmode.c \
	firmware/lib/tpm_lite/mocked_tlcl.c
endif

ifeq ($(FIRMWARE_ARCH),)
# Include stub into firmware lib if compiling for host
FWLIB_SRCS += \
	firmware/stub/tpm_lite_stub.c \
	firmware/stub/utility_stub.c \
	firmware/stub/vboot_api_stub.c \
	firmware/stub/vboot_api_stub_disk.c
endif

FWLIB_OBJS = $(FWLIB_SRCS:%.c=${BUILD}/%.o)
ALL_SRCS += ${FWLIB_SRCS}

fwlib : $(FWLIB)

ifeq ($(FIRMWARE_ARCH),)
# Link test ensures firmware lib doesn't rely on outside libraries
${BUILD}/firmware/fwlib_linktest : firmware/linktest/main.c $(FWLIB)
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(FWLIB)

fwlib : ${BUILD}/firmware/fwlib_linktest
endif

$(FWLIB) : $(FWLIB_OBJS)
	@printf "    RM            $(subst $(BUILD)/,,$(@))\n"
	$(Q)rm -f $@
	@printf "    AR            $(subst $(BUILD)/,,$(@))\n"
	$(Q)ar qc $@ $^

# -----------------------------------------------------------------------------
# Host library

hostlib : $(HOSTLIB) ${BUILD}/host/hostlib_linktest

${BUILD}/host/% ${HOSTLIB} : INCLUDES += \
	-Ihost/include\
	-Ihost/arch/$(ARCH)/include

HOSTLIB_SRCS = \
	host/arch/$(ARCH)/lib/crossystem_arch.c \
	host/lib/crossystem.c \
	host/lib/file_keys.c \
	host/lib/fmap.c \
	host/lib/host_common.c \
	host/lib/host_key.c \
	host/lib/host_keyblock.c \
	host/lib/host_misc.c \
	host/lib/host_signature.c \
	host/lib/signature_digest.c

HOSTLIB_OBJS = $(HOSTLIB_SRCS:%.c=${BUILD}/%.o)
ALL_SRCS += ${HOSTLIB_SRCS}

${BUILD}/host/hostlib_linktest : host/linktest/main.c $(HOSTLIB) $(CRYPTO_LIBS)
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(HOSTLIB) $(CRYPTO_LIBS)

$(HOSTLIB) : $(HOSTLIB_OBJS) $(FWLIB)
	@printf "    RM            $(subst $(BUILD)/,,$(@))\n"
	$(Q)rm -rf $@ $(BUILD)/host/.tmp
	$(Q)mkdir -p $(BUILD)/host/.tmp
	@printf "    AR x          $(subst $(BUILD)/,,$(FWLIB))\n"
	$(Q)cd $(BUILD)/host/.tmp ; ar x $(FWLIB)
	@printf "    AR            $(subst $(BUILD)/,,$(@))\n"
	$(Q)ar qc $@ $^ $(BUILD)/host/.tmp/*.o

# -----------------------------------------------------------------------------
# CGPT library and utility

CGPT = ${BUILD}/cgpt/cgpt
CGPTLIB = ${BUILD}/cgpt/libcgpt-cc.a

CGPT_SRCS = \
	cgpt/cgpt.c \
	cgpt/cgpt_create.c \
	cgpt/cgpt_add.c \
	cgpt/cgpt_boot.c \
	cgpt/cgpt_show.c \
	cgpt/cgpt_repair.c \
	cgpt/cgpt_prioritize.c \
	cgpt/cgpt_find.c \
	cgpt/cgpt_legacy.c \
	cgpt/cmd_show.c \
	cgpt/cmd_repair.c \
	cgpt/cmd_create.c \
	cgpt/cmd_add.c \
	cgpt/cmd_boot.c \
	cgpt/cmd_find.c \
	cgpt/cmd_prioritize.c \
	cgpt/cmd_legacy.c \
	cgpt/cgpt_common.c

CGPT_OBJS = $(CGPT_SRCS:%.c=${BUILD}/%.o)
ALL_SRCS += ${CGPT_SRCS}

# TODO: should link against (or absorb parts of) firmware lib rather
# than recompiling firmware sources
CGPTLIB_SRCS = \
	cgpt/CgptManager.cc \
	cgpt/cgpt_create.c \
	cgpt/cgpt_add.c \
	cgpt/cgpt_boot.c \
	cgpt/cgpt_show.c \
	cgpt/cgpt_repair.c \
	cgpt/cgpt_prioritize.c \
	cgpt/cgpt_common.c \
	firmware/lib/cgptlib/crc32.c \
	firmware/lib/cgptlib/cgptlib_internal.c \
	firmware/stub/utility_stub.c

CGPTLIB_OBJS = $(filter %.o, \
	$(CGPTLIB_SRCS:%.c=${BUILD}/%.o) \
	$(CGPTLIB_SRCS:%.cc=${BUILD}/%.o))
CGPTLIB_DEPS += $(CGPTLIB_OBJS:%.o=%.o.d)
# TODO: add CGPTLIB_DEPS into ALL_DEPS?

cgpt : $(CGPT)

libcgpt_cc : cgpt $(CGPTLIB)

$(CGPTLIB) : INCLUDES += -Ifirmware/lib/cgptlib/include
$(CGPTLIB) : $(CGPTLIB_OBJS) $(HOSTLIB)
	@printf "    RM            $(subst $(BUILD)/,,$(@))\n"
	$(Q)rm -f $@
	@printf "    AR            $(subst $(BUILD)/,,$(@))\n"
	$(Q)ar qc $@ $^

$(CGPT) : INCLUDES += -Ifirmware/lib/cgptlib/include
$(CGPT) : LIBS = $(HOSTLIB)
$(CGPT) : LDLIBS += -luuid
$(CGPT) : LDFLAGS += -static
$(CGPT) : $(CGPT_OBJS) $(HOSTLIB)
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) -o $(CGPT) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) $(LDLIBS)

cgpt_install: $(CGPT)
	mkdir -p $(DESTDIR)
	cp -f $^ $(DESTDIR)
	chmod a+rx $(patsubst ${BUILD}/cgpt/%,$(DESTDIR)/%,$^)

# -----------------------------------------------------------------------------
# Utilities

${BUILD}/utility/% : INCLUDES += -Ihost/include -Iutility/include
${BUILD}/utility/% : CFLAGS += $(PC_CFLAGS)

# All utilities link against host and crypto libs, unless overridden.
# TODO: specify crypto libs separately so we can depend on LIBS?
${BUILD}/utility/% : LIBS = $(HOSTLIB)
${BUILD}/utility/% : LDLIBS += $(CRYPTO_LIBS)

AU_NAMES = \
	crossystem \
	dump_fmap \
	gbb_utility
AU_BINS := $(addprefix ${BUILD}/utility/,$(AU_NAMES))

# Utilities for auto-update toolkits must be statically linked, and don't
# use the crypto libs.
${AU_BINS} : LDFLAGS += -static
${AU_BINS} : CRYPTO_LIBS =

# Scripts to install
UTIL_SCRIPTS = \
	utility/dev_debug_vboot \
	utility/dev_make_keypair \
	utility/enable_dev_usb_boot \
	utility/vbutil_what_keys

UTIL_NAMES = $(AU_NAMES) \
	dev_sign_file \
	dump_kernel_config \
	dumpRSAPublicKey \
	load_kernel_test \
	mount-encrypted \
	pad_digest_utility \
	signature_digest_utility \
	tpm_init_temp_fix \
	tpmc \
	vbutil_ec \
	vbutil_firmware \
	vbutil_kernel \
	vbutil_key \
	vbutil_keyblock \
	verify_data

ifeq ($(MINIMAL),)
UTIL_NAMES += \
	bmpblk_font \
	bmpblk_utility \
	eficompress \
	efidecompress
endif

UTIL_BINS = $(addprefix ${BUILD}/utility/,$(UTIL_NAMES))
ALL_DEPS += $(addsuffix .d,${UTIL_BINS})

utils : $(UTIL_BINS)
# TODO: change ebuild to pull scripts directly out of utility dir
	cp -f $(UTIL_SCRIPTS) $(BUILD)/utility
	chmod a+rx $(patsubst %,$(BUILD)/%,$(UTIL_SCRIPTS))

utils_install : $(UTIL_BINS) $(UTIL_SCRIPTS)
	mkdir -p $(DESTDIR)
	cp -f $(UTIL_BINS) $(DESTDIR)
	chmod a+rx $(patsubst %,$(DESTDIR)/%,$(UTIL_NAMES))
	cp -f $(UTIL_SCRIPTS) $(DESTDIR)
	chmod a+rx $(patsubst utility/%,$(DESTDIR)/%,$(UTIL_SCRIPTS))

${BUILD}/utility/dump_kernel_config : LIBS += $(DUMPKERNELCONFIGLIB)
${BUILD}/utility/dump_kernel_config : \
		${BUILD}/utility/dump_kernel_config_main.o \
		$(HOSTLIB) $(DUMPKERNELCONFIGLIB)
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

${BUILD}/utility/eficompress : CFLAGS += -DSTANDALONE

${BUILD}/utility/efidecompress : CFLAGS += -DSTANDALONE

${BUILD}/utility/gbb_utility : CFLAGS += -DWITH_UTIL_MAIN

${BUILD}/utility/crossystem : ${BUILD}/utility/crossystem_main.o $(HOSTLIB)
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS) $(LDLIBS)

${BUILD}/utility/bmpblk_utility.o : CFLAGS += -DWITH_UTIL_MAIN

${BUILD}/utility/bmpblk_utility : LDLIBS = -llzma -lyaml
${BUILD}/utility/bmpblk_utility : \
		${BUILD}/utility/bmpblk_utility.o \
		${BUILD}/utility/bmpblk_util.o \
		${BUILD}/utility/image_types.o \
		${BUILD}/utility/eficompress.o \
		${BUILD}/utility/efidecompress.o
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CXX) $(CFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

${BUILD}/utility/bmpblk_font: $(HOSTLIB) \
		${BUILD}/utility/bmpblk_font.o \
		${BUILD}/utility/image_types.o
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

# The embedded libcrypto conflicts with the shipped openssl,
# so this builds without the common CFLAGS (and those includes).

${BUILD}/utility/mount-helpers.o: \
		utility/mount-helpers.c \
		utility/mount-helpers.h \
		utility/mount-encrypted.h
	@printf "    CCm-e         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) -Wall -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector \
		$(shell $(PKG_CONFIG) --cflags glib-2.0 openssl) \
		-c $< -o $@

${BUILD}/utility/mount-encrypted: $(LIBS) \
		utility/mount-encrypted.c \
		utility/mount-encrypted.h \
		${BUILD}/utility/mount-helpers.o
	@printf "    CCm-exe       $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) -Wall -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector \
		$(shell $(PKG_CONFIG) --cflags glib-2.0 openssl) \
		-Ifirmware/include \
		-Ihost/include \
		$(LDFLAGS) \
		$< -o $@ \
		${BUILD}/utility/mount-helpers.o $(LIBS) \
		$(shell $(PKG_CONFIG) --libs glib-2.0 openssl) \
		-lm

# TODO: fix load_firmware_test util; it never got refactored for the new APIs

# Utility to generate TLCL structure definition header file.

${BUILD}/utility/tlcl_generator : CFLAGS += -fpack-struct
${BUILD}/utility/tlcl_generator : LIBS =

STRUCTURES_TMP=${BUILD}/tlcl_structures.tmp
STRUCTURES_SRC=firmware/lib/tpm_lite/include/tlcl_structures.h

update_tlcl_structures: ${BUILD}/utility/tlcl_generator
	@printf "    Rebuilding TLCL structures\n"
	$(Q)${BUILD}/utility/tlcl_generator > $(STRUCTURES_TMP)
	$(Q)cmp -s $(STRUCTURES_TMP) $(STRUCTURES_SRC) || \
		( echo "%% Updating structures.h %%" && \
		  cp $(STRUCTURES_TMP) $(STRUCTURES_SRC) )

# -----------------------------------------------------------------------------
# Library to dump kernel config

libdump_kernel_config: $(DUMPKERNELCONFIGLIB)

$(DUMPKERNELCONFIGLIB) : ${BUILD}/utility/dump_kernel_config.o
	@printf "    RM            $(subst $(BUILD)/,,$(@))\n"
	$(Q)rm -f $@
	@printf "    AR            $(subst $(BUILD)/,,$(@))\n"
	$(Q)ar qc $@ $^

# -----------------------------------------------------------------------------
# Tests

# Allow multiple definitions, so tests can mock functions from other libraries
${BUILD}/tests/% : CFLAGS += -Xlinker --allow-multiple-definition
${BUILD}/tests/% : INCLUDES += -Ihost/include
${BUILD}/tests/% : LDLIBS += $(CRYPTO_LIBS) -lrt -luuid
${BUILD}/tests/% : LIBS += $(HOSTLIB) $(TEST_LIB)

TEST_NAMES = \
	cgptlib_test \
	CgptManagerTests \
	rollback_index2_tests \
	rsa_padding_test \
	rsa_utility_tests \
	rsa_verify_benchmark \
	sha_benchmark \
	sha_tests \
	stateful_util_tests \
	tpm_bootmode_tests \
	utility_string_tests \
	utility_tests \
	vboot_nvstorage_test \
	vboot_api_init_tests \
	vboot_api_devmode_tests \
	vboot_api_firmware_tests \
	vboot_api_kernel_tests \
	vboot_audio_tests \
	vboot_common_tests \
	vboot_common2_tests \
	vboot_common3_tests \
	vboot_ec_tests \
	vboot_firmware_tests

TLCL_TEST_NAMES = \
	tpmtest_earlyextend \
	tpmtest_earlynvram \
        tpmtest_earlynvram2 \
	tpmtest_enable \
	tpmtest_fastenable \
	tpmtest_globallock \
        tpmtest_redefine_unowned \
        tpmtest_spaceperm \
	tpmtest_testsetup \
	tpmtest_timing \
        tpmtest_writelimit
TEST_NAMES += $(addprefix tpm_lite/,$(TLCL_TEST_NAMES))

TEST_BINS = $(addprefix ${BUILD}/tests/,$(TEST_NAMES))
ALL_DEPS += $(addsuffix .d,${TEST_BINS})

tests : $(TEST_BINS)

${TEST_LIB}: \
		${BUILD}/tests/test_common.o \
		${BUILD}/tests/timer_utils.o \
		${BUILD}/tests/crc32_test.o
	@printf "    RM            $(subst $(BUILD)/,,$(@))\n"
	$(Q)rm -f $@
	@printf "    AR            $(subst $(BUILD)/,,$(@))\n"
	$(Q)ar qc $@ $^

# Compile rollback_index.c for unit test, so it uses the same implementation
# as it does in the firmware.
${BUILD}/tests/rollback_index_for_test.o : CFLAGS += -DROLLBACK_UNITTEST
${BUILD}/tests/rollback_index_for_test.o : firmware/lib/rollback_index.c
	@printf "    CC            $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

${BUILD}/tests/rollback_index2_tests: $(LIBS) \
		${BUILD}/tests/rollback_index2_tests.o \
		${BUILD}/tests/rollback_index_for_test.o
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

${BUILD}/tests/vboot_audio_for_test.o : CFLAGS += -DCUSTOM_MUSIC
${BUILD}/tests/vboot_audio_for_test.o : firmware/lib/vboot_audio.c
	@printf "    CC            $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

${BUILD}/tests/vboot_audio_tests: $(LIBS) \
		${BUILD}/tests/vboot_audio_for_test.o \
		${BUILD}/tests/vboot_audio_tests.o
	@printf "    CCexe         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

# TODO: only needed for AU_TARGETS ebuild?
cgptmanager_tests: ${BUILD}/tests/CgptManagerTests

${BUILD}/tests/CgptManagerTests: CFLAGS += -DWITH_UTIL_MAIN $(PC_CFLAGS)
${BUILD}/tests/CgptManagerTests: LDLIBS += -lgtest -lgflags $(PC_LDLIBS)
${BUILD}/tests/CgptManagerTests: LIBS += $(CGPTLIB)
${BUILD}/tests/CgptManagerTests: $(HOSTLIB) $(CGPTLIB) $(TEST_LIB) \
		tests/CgptManagerTests.cc
	@printf "    CXXexe        $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

${BUILD}/tests/rollback_index_test.o : INCLUDES += -I/usr/include
${BUILD}/tests/rollback_index_test : LIBS += -ltlcl

# TODO: port these tests to new API, if not already eqivalent
# functionality in other tests.  These don't even compile at present.
#
#		big_firmware_tests
#		big_kernel_tests
#		firmware_image_tests
#		firmware_rollback_tests
#		firmware_splicing_tests
#		firmware_verify_benchmark
#		kernel_image_tests
#		kernel_rollback_tests
#		kernel_splicing_tests
#		kernel_verify_benchmark
#		rollback_index_test
#		verify_firmware_fuzz_driver
#		verify_kernel_fuzz_driver

# -----------------------------------------------------------------------------
# Targets to run tests

# Frequently-run tests
runtests : runbmptests runcgpttests runfuzztests runmisctests

# Generate test keys
genkeys:
	tests/gen_test_keys.sh

# Generate test cases for fuzzing
genfuzztestcases:
	tests/gen_fuzz_test_cases.sh

runbmptests: utils
	cd tests/bitmaps && BMPBLK=${BUILD}/utility/bmpblk_utility \
		./TestBmpBlock.py -v

runcgpttests : cgpt tests
	${BUILD}/tests/cgptlib_test
	${BUILD}/tests/CgptManagerTests --v=1
	tests/run_cgpt_tests.sh ${BUILD}/cgpt/cgpt

# Exercise vbutil_kernel and vbutil_firmware
runfuzztests: genfuzztestcases utils tests
	tests/run_preamble_tests.sh
	tests/run_vbutil_kernel_arg_tests.sh

runmisctests : tests utils
	${BUILD}/tests/rollback_index2_tests
	${BUILD}/tests/rsa_utility_tests
	${BUILD}/tests/sha_tests
	${BUILD}/tests/stateful_util_tests
	${BUILD}/tests/tpm_bootmode_tests
	${BUILD}/tests/utility_string_tests
	${BUILD}/tests/utility_tests
	${BUILD}/tests/vboot_api_devmode_tests
	${BUILD}/tests/vboot_api_init_tests
	${BUILD}/tests/vboot_api_firmware_tests
	${BUILD}/tests/vboot_audio_tests
	${BUILD}/tests/vboot_firmware_tests
	tests/run_rsa_tests.sh
	tests/run_vboot_common_tests.sh
	tests/run_vbutil_tests.sh

# Run long tests, including all permutations of encryption keys (instead of
# just the ones we use) and tests of currently-unused code (e.g. vboot_ec).
# Not run by automated build.
runlongtests : genkeys genfuzztestcases tests utils
	tests/run_preamble_tests.sh --all
	tests/run_vboot_common_tests.sh --all
	tests/run_vboot_ec_tests.sh
	tests/run_vbutil_tests.sh --all

# TODO: tests to run when ported to new API
#	./run_image_verification_tests.sh
#	# Splicing tests
#	${BUILD}/tests/firmware_splicing_tests
#	${BUILD}/tests/kernel_splicing_tests
#	# Rollback Tests
#	${BUILD}/tests/firmware_rollback_tests
#	${BUILD}/tests/kernel_rollback_tests

# -----------------------------------------------------------------------------
# Build rules

# TODO: why do we need to specify $(HOSTLIB) here directly instead of $(LIBS)?

${BUILD}/utility/% : utility/%.c $(HOSTLIB)
	@printf "    CCutil        $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $< -o $@ $(LIBS) $(LDLIBS)

${BUILD}/utility/% : utility/%.cc $(HOSTLIB)
	@printf "    CXXutil       $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $< -o $@ $(LIBS) $(LDLIBS)

${BUILD}/tests/% : tests/%.c $(HOSTLIB) $(TEST_LIB)
	@printf "    CCtest        $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $< -o $@ $(LIBS) $(LDLIBS)

${BUILD}/tests/% : tests/%.cc $(HOSTLIB) $(TEST_LIB)
	@printf "    CXXtest       $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $< -o $@ $(LIBS) $(LDLIBS)

${BUILD}/tests/tpm_lite/tpmtest_% : tests/tpm_lite/%.c $(HOSTLIB) $(TEST_LIB) \
		${BUILD}/tests/tpm_lite/tlcl_tests.o
	@printf "    CCtpm         $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $^ -o $@ $(LIBS) $(LDLIBS)

${BUILD}/%.o : %.c
	@printf "    CC            $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

${BUILD}/%.o : %.cc
	@printf "    CXX           $(subst $(BUILD)/,,$(@))\n"
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# -----------------------------------------------------------------------------
# Dependencies must come last after ALL_SRC has been accumulated

ALL_OBJS = $(ALL_SRCS:%.c=${BUILD}/%.o)
ALL_DEPS += $(ALL_OBJS:%.o=%.o.d)

-include ${ALL_DEPS}
