# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LDFLAGS += -luuid -static
BUILD_ROOT := ${BUILD}/cgpt

INCLUDES = -I$(FWDIR)/lib/cgptlib/include -I$(FWDIR)/include
LIBS = ${HOSTLIB}

DESTDIR ?= /usr/bin

PROGNAME = ${BUILD_ROOT}/cgpt
LIB_CGPT_CC = ${BUILD_ROOT}/libcgpt-cc.a

COMMON_SRCS = \
	cgpt_common.c \
	cgpt_create.c \
	cgpt_add.c \
	cgpt_boot.c \
	cgpt_show.c \
	cgpt_repair.c \
	cgpt_prioritize.c \
	cgpt_find.c

ALL_SRCS = \
	$(COMMON_SRCS) \
	cgpt.c \
	cmd_show.c \
	cmd_repair.c \
	cmd_create.c \
	cmd_add.c \
	cmd_boot.c \
	cmd_find.c \
	cmd_prioritize.c

LIB_CGPT_CC_SRCS = \
	$(COMMON_SRCS) \
	CgptManager.cc

main: $(PROGNAME) $(LIB_CGPT_CC)

LIB_CGPT_CC_OBJS = $(LIB_CGPT_CC_SRCS:%.c=${BUILD_ROOT}/%.o) $(LIB_CGPT_CC_SRCS:%.cc=${BUILD_ROOT}/%.o)

include ../common.mk

$(PROGNAME): $(ALL_OBJS) $(LIBS)
	$(CC) -o $(PROGNAME) $(CFLAGS) $^ $(LDFLAGS)

$(BUILD_ROOT)/CgptManager.o: CgptManager.cc
	$(CXX) $(CFLAGS) $(INCLUDES) -c $^ -o $@

$(LIB_CGPT_CC): $(LIB_CGPT_CC_OBJS)
	rm -f $@
	ar qc $@ $^

install: $(PROGNAME)
	mkdir -p $(DESTDIR)
	cp -f $^ $(DESTDIR)
	chmod a+rx $(patsubst ${BUILD_ROOT}/%,$(DESTDIR)/%,$^)

.PHONY: all install
