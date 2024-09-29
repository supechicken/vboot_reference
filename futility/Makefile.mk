# Copyright 2024 The ChromiumOS Authors

utillib-y += \
	dump_kernel_config_lib.c

futil-cflags += -Ihost/lib21/include

# Avoid build failures outside the chroot on Ubuntu 2022.04
# e.g.:
# futility/cmd_create.c:161:9: warning: ‘RSA_free’ is deprecated: Since OpenSSL 3.0
# [-Wdeprecated-declarations]
ifeq ($(OPENSSL_VERSION),3)
futil-cflags += -Wno-error=deprecated-declarations
endif

futil-y += \
	cmd_create.c \
	cmd_dump_fmap.c \
	cmd_dump_kernel_config.c \
	cmd_flash_util.c \
	cmd_gbb_utility.c \
	cmd_gscvd.c \
	cmd_load_fmap.c \
	cmd_pcr.c \
	cmd_read.c \
	cmd_show.c \
	cmd_sign.c \
	cmd_update.c \
	cmd_vbutil_firmware.c \
	cmd_vbutil_kernel.c \
	cmd_vbutil_keyblock.c \
	cmd_vbutil_key.c \
	file_type_bios.c \
	file_type.c \
	file_type_rwsig.c \
	file_type_usbpd1.c \
	flash_helpers.c \
	futility.c \
	misc.c \
	platform_csme.c \
	vb1_helper.c \
	vb2_helper.c \

futil-$(if $(filter-out 0,${USE_FLASHROM}),y) += \
	updater_utils.c \
	updater_quirks.c \
	updater_manifest.c \
	updater_dut.c \
	updater_archive.c \
	updater.c

FUTIL_CMD_LIST = ${BUILD}/gen/futility_cmds.c
futil-y += ${FUTIL_CMD_LIST}

# Generates the list of commands defined in futility by running grep in the
# source files looking for the DECLARE_FUTIL_COMMAND() macro usage.
${FUTIL_CMD_LIST}: $$(futil-srcs)
	@${PRINTF} "    GEN           $(subst ${BUILD}/,,$@)\n"
	${Q}rm -f $@ $@_t $@_commands
	${Q}mkdir -p ${BUILD}/gen
	${Q}grep -hoRE '^DECLARE_FUTIL_COMMAND\([^,]+' $^ \
		| sed 's/DECLARE_FUTIL_COMMAND(\(.*\)/_CMD(\1)/' \
		| sort >>$@_commands
	${Q}./scripts/getversion.sh >> $@_t
	${Q}echo '#define _CMD(NAME) extern const struct' \
		'futil_cmd_t __cmd_##NAME;' >> $@_t
	${Q}cat $@_commands >> $@_t
	${Q}echo '#undef _CMD' >> $@_t
	${Q}echo '#define _CMD(NAME) &__cmd_##NAME,' >> $@_t
	${Q}echo 'const struct futil_cmd_t *const futil_cmds[] = {' >> $@_t
	${Q}cat $@_commands >> $@_t
	${Q}echo '0};  /* null-terminated */' >> $@_t
	${Q}echo '#undef _CMD' >> $@_t
	${Q}mv $@_t $@
	${Q}rm -f $@_commands

hostlib-y += dump_kernel_config_lib.c
