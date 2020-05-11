# Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
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
#

# Supported OSs by architecture
AMD64_TARGETS := ubuntu20.04 ubuntu18.04 ubuntu16.04 debian10 debian9
X86_64_TARGETS := centos7 rhel8 amazonlinux1 amazonlinux2 opensuse-leap15.1
PPC64LE_TARGETS := ubuntu18.04 ubuntu16.04 centos7 rhel8
ARM64_TARGETS := ubuntu18.04
AARCH64_TARGETS := rhel8

# Define top-level build targets
docker%: SHELL:=/bin/bash

# Native targets (for tab completion by OS name only on native platform)
ifeq ($(PLATFORM),x86_64)
NATIVE_TARGETS := $(AMD64_TARGETS) $(X86_64_TARGETS)
$(AMD64_TARGETS): %: %-amd64
$(X86_64_TARGETS): %: %-x86_64
else ifeq ($(PLATFORM),ppc64le)
NATIVE_TARGETS := $(PPC64LE_TARGETS)
$(PPC64LE_TARGETS): %: %-ppc64le
else ifeq ($(PLATFORM),aarch64)
NATIVE_TARGETS := $(ARM64_TARGETS) $(AARCH64_TARGETS)
$(ARM64_TARGETS): %: %-arm64
$(AARCH64_TARGETS): %: %-aarch64
endif
docker-native: $(NATIVE_TARGETS)

# amd64 targets
AMD64_TARGETS := $(patsubst %, %-amd64, $(AMD64_TARGETS))
$(AMD64_TARGETS): ARCH := amd64
$(AMD64_TARGETS): %: --%
docker-amd64: $(AMD64_TARGETS)

# x86_64 targets
X86_64_TARGETS := $(patsubst %, %-x86_64, $(X86_64_TARGETS))
$(X86_64_TARGETS): ARCH := x86_64
$(X86_64_TARGETS): %: --%
docker-x86_64: $(X86_64_TARGETS)

# arm64 targets
ARM64_TARGETS := $(patsubst %, %-arm64, $(ARM64_TARGETS))
$(ARM64_TARGETS): ARCH := arm64
$(ARM64_TARGETS): %: --%
docker-arm64: $(ARM64_TARGETS)

# aarch64 targets
AARCH64_TARGETS := $(patsubst %, %-aarch64, $(AARCH64_TARGETS))
$(AARCH64_TARGETS): ARCH := aarch64
$(AARCH64_TARGETS): %: --%
docker-aarch64: $(AARCH64_TARGETS)

# ppc64le targets
PPC64LE_TARGETS := $(patsubst %, %-ppc64le, $(PPC64LE_TARGETS))
$(PPC64LE_TARGETS): ARCH := ppc64le
$(PPC64LE_TARGETS): WITH_LIBELF := yes
$(PPC64LE_TARGETS): %: --%
docker-ppc64le: $(PPC64LE_TARGETS)

# docker target to build for all os/arch combinations
docker-all: $(AMD64_TARGETS) $(X86_64_TARGETS) \
            $(ARM64_TARGETS) $(AARCH64_TARGETS) \
            $(PPC64LE_TARGETS)

# Default variables for all private '--' targets below.
# One private target is defined for each OS we support.
--%: TARGET_PLATFORM = $(*)
--%: VERSION = $(patsubst $(OS)%-$(ARCH),%,$(TARGET_PLATFORM))
--%: BASEIMAGE = $(OS):$(VERSION)
--%: BUILDIMAGE = nvidia/$(LIB_NAME)/$(OS)$(VERSION)-$(ARCH)
--%: DOCKERFILE = $(MAKE_DIR)/Dockerfile.$(OS)
--%: ARTIFACTS_DIR = $(DIST_DIR)/$(OS)$(VERSION)/$(ARCH)
--%: docker-build-%
	@

# private verify target to be customized
# by specific targets below (if desired)
--%-verify:
	@

# private OS targets with defaults
--ubuntu%: OS := ubuntu
--debian%: OS := debian
--centos%: OS := centos
--amazonlinux%: OS := amazonlinux

# private opensuse-leap target with overrides
--opensuse-leap%: OS := opensuse-leap
--opensuse-leap%: BASEIMAGE = opensuse/leap:$(VERSION)

# private rhel target with extra variables and overrides
--rhel%: OS = rhel
--rhel%: BASEIMAGE = registry.access.redhat.com/ubi$(VERSION)
--rhel%: RHEL_CREDENTIALS_FILE ?= $(CURDIR)/rhel-credentials.env
--rhel%: EXTRA_BUILD_ARGS = --secret id=rhel-credentials,src=$(RHEL_CREDENTIALS_FILE)

--rhel%-verify:
	@if [ ! -f "$(RHEL_CREDENTIALS_FILE)" ]; then \
	     echo "Error: Missing \$$RHEL_CREDENTIALS_FILE"; \
	     echo ""; \
	     echo "In order to build for rhel platforms, you need to setup a \$$RHEL_CREDENTIALS_FILE"; \
	     echo "This file should contain the following 2 lines:"; \
	     echo ""; \
	     echo "  RHEL_USERNAME=<username>"; \
	     echo "  RHEL_PASSWORD=<password>"; \
	     echo ""; \
	     echo "By default this file will be searched for at:"; \
	     echo ""; \
	     echo "  $(CURDIR)/rhel-credentials.env"; \
	     echo ""; \
	     echo "Otherwise you can set an environment variable called \$$RHEL_CREDENTIALS_FILE"; \
	     echo "to point to a file of your choice."; \
	     echo ""; \
	     false; \
	 fi

docker-build-%: --%-verify
	@echo "Building for $(TARGET_PLATFORM)"
	docker pull --platform=linux/$(ARCH) $(BASEIMAGE)
	DOCKER_BUILDKIT=1 \
	$(DOCKER) build \
	    --progress=plain \
	    --build-arg BASEIMAGE=$(BASEIMAGE) \
	    --build-arg OS_VERSION=$(VERSION) \
	    --build-arg OS_ARCH=$(ARCH) \
	    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
	    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
	    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
	    $(EXTRA_BUILD_ARGS) \
	    --tag $(BUILDIMAGE) \
	    --file $(DOCKERFILE) .
	$(DOCKER) run \
	    -e DISTRIB \
	    -e SECTION \
	    -v $(ARTIFACTS_DIR):/dist \
	    $(BUILDIMAGE)

docker-clean:
	IMAGES=$$(docker images "nvidia/$(LIB_NAME)/*" --format="{{.ID}}"); \
	if [ "$${IMAGES}" != "" ]; then \
	    docker rmi -f $${IMAGES}; \
	fi
