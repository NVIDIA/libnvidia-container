#
# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
#

include $(MAKE_DIR)/common.mk

##### BSD make compat #####

export MKSHARE   := no
export MKPROFILE := no
export LORDER    := echo
export INSTALL   := $(INSTALL) -D
export BINOWN    := $(shell id -u)
export BINGRP    := $(shell id -g)
export LIBDIR    := $(libdir)
export INCSDIR   := $(includedir)

##### Source definitions #####

VERSION  := 0.7.1
PREFIX   := elftoolchain-$(VERSION)
URL      := https://sourceforge.net/projects/elftoolchain/files/Sources/$(PREFIX)/$(PREFIX).tar.bz2

SRCS_DIR := $(DEPS_DIR)/src/$(PREFIX)
LIBELF   := $(SRCS_DIR)/libelf
COMMON   := $(SRCS_DIR)/common

##### Flags definitions #####

export CPPFLAGS := -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
export CFLAGS   := -O2 -g -fdata-sections -ffunction-sections -fstack-protector -fno-strict-aliasing -fPIC

##### Private rules #####

$(SRCS_DIR)/.download_stamp:
	$(MKDIR) -p $(SRCS_DIR)
	$(CURL) --progress-bar -fSL $(URL) | \
	$(TAR) -C $(SRCS_DIR) --strip-components=1 -xj $(addprefix $(PREFIX)/,mk common libelf)
	$(CP) $(MAKE_DIR)/native-elf-format $(COMMON)
	@touch $@

$(SRCS_DIR)/.build_stamp: $(SRCS_DIR)/.download_stamp
	$(BMAKE) -j $(shell nproc) -C $(COMMON)
	$(BMAKE) -j $(shell nproc) -C $(LIBELF)
	@touch $@

##### Public rules #####

.PHONY: all install clean $(COMMON) $(LIBELF)

all: $(SRCS_DIR)/.build_stamp

unexport DESTDIR
install: all
	$(BMAKE) -C $(COMMON) install INCSDIR=$(INCSDIR) DESTDIR=$(DESTDIR)
	$(BMAKE) -C $(LIBELF) install INCSDIR=$(INCSDIR) DESTDIR=$(DESTDIR)

clean:
	$(RM) $(SRCS_DIR)/.build_stamp
	$(BMAKE) -C $(LIBELF) clean
