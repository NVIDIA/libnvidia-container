#
# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
#

##### Global definitions #####

MV       ?= mv -f
CP       ?= cp -a
LN       ?= ln
TAR      ?= tar
CURL     ?= curl
MKDIR    ?= mkdir
LDCONFIG ?= ldconfig
INSTALL  ?= install
STRIP    ?= strip
OBJCPY   ?= objcopy
RPCGEN   ?= rpcgen
BMAKE    ?= MAKEFLAGS= bmake
DOCKER   ?= docker

UID      := $(shell id -u)
GID      := $(shell id -g)
ifdef SOURCE_DATE_EPOCH
    DATE := $(shell date -u -d "@$(SOURCE_DATE_EPOCH)" --iso-8601=minutes)
else
    DATE := $(shell date -u --iso-8601=minutes)
endif
REVISION := $(shell git rev-parse HEAD)
COMPILER := $(realpath $(shell which $(CC)))
PLATFORM ?= $(shell uname -m)

ifeq ($(DATE),)
$(error Invalid date format)
endif
ifeq ($(REVISION),)
$(error Invalid commit hash)
endif
ifeq ($(COMPILER),)
$(error Invalid compiler)
endif

##### Function definitions #####

getdef = $(shell sed -n "0,/$(1)/s/\#define\s\+$(1)\s\+\(\w*\)/\1/p" $(2))

ifeq ($(PLATFORM),x86_64)
getarch = $(shell [ -f /etc/debian_version ] && echo "amd64" || echo "x86_64")
else ifeq ($(PLATFORM),ppc64le)
getarch = $(shell [ -f /etc/debian_version ] && echo "ppc64el" || echo "ppc64le")
else ifeq ($(PLATFORM),aarch64)
getarch = $(shell [ -f /etc/debian_version ] && echo "arm64" || echo "aarch64")
else
$(error Unsupported architecture)
endif
