# Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

GO ?= go
RM ?= rm
MKDIR ?= mkdir
INSTALL ?= install

DESTDIR ?=
VERSION ?= devel

PKGNAME := nvcgo
LIBNAME := libnvidia-container-go
OBJNAME := $(LIBNAME).so
HDRNAME := $(LIBNAME).h
SONAME  := $(LIBNAME).so.1
DSTNAME := $(LIBNAME).so.$(VERSION)
CTYPESH := ctypes.h

CGO_CFLAGS  := -std=gnu11 -O2
CGO_LDFLAGS := -Wl,--gc-sections -Wl,-s -Wl,-soname,$(SONAME)

build: $(OBJNAME)

$(OBJNAME): $(wildcard $(CURDIR)/*.go) $(wildcard */*.go)
	export CGO_CFLAGS="$(CGO_CFLAGS)"; \
	export CGO_LDFLAGS="$(CGO_LDFLAGS)"; \
	$(GO) build -o $(@) -ldflags "-s -w" -buildmode=c-shared .

install: $(OBJNAME)
	$(MKDIR) -p $(DESTDIR)/usr/local/lib
	$(MKDIR) -p $(DESTDIR)/usr/local/include/$(PKGNAME)
	$(INSTALL) $(OBJNAME) $(DESTDIR)/usr/local/lib/$(DSTNAME)
	$(INSTALL) $(HDRNAME) $(DESTDIR)/usr/local/include/$(PKGNAME)/$(PKGNAME).h
	$(INSTALL) $(CTYPESH) $(DESTDIR)/usr/local/include/$(PKGNAME)/$(CTYPESH)

clean:
	$(RM) -f $(SONAME) $(HDRNAME)

.PHONY: build clean install
