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

include $(MAKE_DIR)/common.mk

##### Source definitions #####

PREFIX         := nvcgo
SRCS_DIR       := $(DEPS_DIR)/src/$(PREFIX)
VERSION        := $(VERSION)

##### Public rules #####

.PHONY: all install clean

build:
	$(RM) -rf $(SRCS_DIR)
	$(CP) -R $(CURDIR)/src/$(PREFIX) $(SRCS_DIR)
	$(MAKE) -C $(SRCS_DIR) VERSION=$(VERSION) clean
	$(MAKE) -C $(SRCS_DIR) VERSION=$(VERSION) build

install: build
	$(MAKE) -C $(SRCS_DIR) install DESTDIR=$(DESTDIR)

clean:
	$(MAKE) -C $(SRCS_DIR) clean
