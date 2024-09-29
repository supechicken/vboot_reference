# Copyright 2024 The ChromiumOS Authors

cgpt-y = \
	cgpt.c \
	cgpt_add.c \
	cgpt_boot.c \
	cgpt_common.c \
	cgpt_create.c \
	cgpt_edit.c \
	cgpt_find.c \
	cgpt_legacy.c \
	cgpt_prioritize.c \
	cgpt_repair.c \
	cgpt_show.c \
	cmd_add.c \
	cmd_boot.c \
	cmd_create.c \
	cmd_edit.c \
	cmd_find.c \
	cmd_legacy.c \
	cmd_prioritize.c \
	cmd_repair.c \
	cmd_show.c

cgpt-wrapper-y +=  \
	cgpt_nor.c \
	cgpt_wrapper.c

hostlib-y += \
	cgpt_add.c \
	cgpt_boot.c \
	cgpt_common.c \
	cgpt_create.c \
	cgpt_edit.c \
	cgpt_find.c \
	cgpt_prioritize.c \
	cgpt_repair.c \
	cgpt_show.c 

ifneq ($(filter-out 0,${GPT_SPI_NOR}),)
cgpt-y += cgpt_nor.c
hostlib-y += cgpt_nor.c
endif

utillib-y += \
	cgpt_add.c \
	cgpt_boot.c \
	cgpt_common.c \
	cgpt_create.c \
	cgpt_edit.c \
	cgpt_prioritize.c \
	cgpt_repair.c \
	cgpt_show.c
