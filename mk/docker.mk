# Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

# Version of golang to use in docker specific builds.
GOLANG_VERSION := 1.17.1

# Global definitions. These are defined here to allow the docker targets to be
# invoked directly without the root makefile.
WITH_NVCGO   ?= yes
WITH_LIBELF  ?= no
WITH_TIRPC   ?= no
WITH_SECCOMP ?= yes

DOCKER       ?= docker
LIB_NAME     ?= libnvidia-container
PLATFORM     ?= $(shell uname -m)

DIST_DIR     ?= $(CURDIR)/dist
MAKE_DIR     ?= $(CURDIR)/mk
REVISION 	 ?= $(shell git rev-parse HEAD)

# Supported OSs by architecture
AMD64_TARGETS := ubuntu20.04 ubuntu18.04 ubuntu16.04 debian10 debian9
X86_64_TARGETS := fedora35 centos7 centos8 rhel7 rhel8 amazonlinux2 opensuse-leap15.3 opensuse-leap15.4
PPC64LE_TARGETS := ubuntu18.04 ubuntu16.04 centos7 centos8 rhel7 rhel8
ARM64_TARGETS := ubuntu18.04
AARCH64_TARGETS := fedora35 centos8 rhel8 amazonlinux2

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
--%: CFLAGS :=
--%: LDLIBS :=
--%: TARGET_PLATFORM = $(*)
--%: VERSION = $(patsubst $(OS)%-$(ARCH),%,$(TARGET_PLATFORM))
--%: BASEIMAGE = $(OS):$(VERSION)
--%: BUILDIMAGE = nvidia/$(LIB_NAME)/$(OS)$(VERSION)-$(ARCH)
--%: DOCKERFILE = $(MAKE_DIR)/Dockerfile.$(OS)
--%: ARTIFACTS_DIR = $(DIST_DIR)/$(OS)$(VERSION)/$(ARCH)
--%: docker-build-%
	@

# Define verify targets to run a minimal sanity check that everything has built
# and runs correctly for a given OS on amd64/x86_64. Requires a working NVIDIA
# driver installation on a native amd64/x86_64 machine.
$(patsubst %, %-verify, $(AMD64_TARGETS)): ARCH := amd64
$(patsubst %, %-verify, $(AMD64_TARGETS)): %-verify: --verify-%
$(patsubst %, %-verify, $(X86_64_TARGETS)): ARCH := x86_64
$(patsubst %, %-verify, $(X86_64_TARGETS)): %-verify: --verify-%
docker-amd64-verify: $(patsubst %, %-verify, $(AMD64_TARGETS)) \
                     $(patsubst %, %-verify, $(X86_64_TARGETS))

--verify-%: docker-verify-%
	@

# private OS targets with defaults
--ubuntu%: OS := ubuntu
--debian%: OS := debian
--amazonlinux%: OS := amazonlinux

# For the ubuntu18.04 arm64 target we add a dependency on libnvidia-container0 to ensure that libnvidia-container-tools also supports Jetson devices
--ubuntu18.04-arm64: LIBNVIDIA_CONTAINER0_DEPENDENCY = libnvidia-container0 (= 0.9.0~beta.1) | libnvidia-container0 (>= 0.10.0+jetpack)

# private centos target with overrides
--centos%: OS := centos
--centos8%: CFLAGS := -I/usr/include/tirpc
--centos8%: LDLIBS := -ltirpc
--centos8%: BASEIMAGE = quay.io/centos/centos:stream8

# private fedora target with overrides
--fedora%: OS := fedora
--fedora%: CFLAGS := -I/usr/include/tirpc
--fedora%: LDLIBS := -ltirpc
# The fedora(35) base image has very slow performance when building aarch64 packages.
# Since our primary concern here is glibc versions, we use the older glibc version available in centos8.
--fedora35%: BASEIMAGE = quay.io/centos/centos:stream8
--fedora35%: OS := centos
# We need to specify this version to ensure that the correct packages are installed in the centos8 build image
--fedora35%: VERSION := 8
--fedora35%: ARTIFACTS_DIR = $(DIST_DIR)/fedora35/$(ARCH)

# private opensuse-leap target with overrides
--opensuse-leap%: OS := opensuse-leap
--opensuse-leap%: BASEIMAGE = opensuse/leap:$(VERSION)

# private rhel target (actually built on centos)
--rhel%: OS := centos
--rhel%: VERSION = $(patsubst rhel%-$(ARCH),%,$(TARGET_PLATFORM))
--rhel%: ARTIFACTS_DIR = $(DIST_DIR)/rhel$(VERSION)/$(ARCH)
--rhel8%: CFLAGS := -I/usr/include/tirpc
--rhel8%: LDLIBS := -ltirpc
--rhel8%: BASEIMAGE = quay.io/centos/centos:stream8

--verify-rhel%: OS := centos
--verify-rhel%: VERSION = $(patsubst rhel%-$(ARCH),%,$(TARGET_PLATFORM))

docker-build-%: $(ARTIFACTS_DIR)
	@echo "Building for $(TARGET_PLATFORM)"
	$(DOCKER) pull --platform=linux/$(ARCH) $(BASEIMAGE)
	DOCKER_BUILDKIT=1 \
	$(DOCKER) build \
	    --platform=linux/$(ARCH) \
	    --progress=plain \
	    --build-arg BASEIMAGE="$(BASEIMAGE)" \
	    --build-arg OS_VERSION="$(VERSION)" \
	    --build-arg OS_ARCH="$(ARCH)" \
	    --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	    --build-arg WITH_NVCGO="$(WITH_NVCGO)" \
	    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
	    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
	    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
	    --build-arg CFLAGS="$(CFLAGS)" \
	    --build-arg LDLIBS="$(LDLIBS)" \
	    --build-arg REVISION="$(REVISION)" \
	    --build-arg LIBNVIDIA_CONTAINER0_DEPENDENCY="$(LIBNVIDIA_CONTAINER0_DEPENDENCY)" \
	    $(EXTRA_BUILD_ARGS) \
	    --tag $(BUILDIMAGE) \
	    --file $(DOCKERFILE) .
	$(DOCKER) run \
	    --platform=linux/$(ARCH) \
	    -e TAG \
	    -v $(ARTIFACTS_DIR):/dist \
	    $(BUILDIMAGE)

docker-verify-%: %
	@echo "Verifying for $(TARGET_PLATFORM)"
	$(DOCKER) run \
	    --privileged \
	    --runtime=nvidia  \
	    -e NVIDIA_VISIBLE_DEVICES=all \
	    --rm $(BUILDIMAGE) \
	    bash -c "make install; LD_LIBRARY_PATH=/usr/local/lib/  nvidia-container-cli -k -d /dev/tty info"

docker-clean:
	IMAGES=$$(docker images "nvidia/$(LIB_NAME)/*" --format="{{.ID}}"); \
	if [ "$${IMAGES}" != "" ]; then \
	    docker rmi -f $${IMAGES}; \
	fi

$(ARTIFACTS_DIR):
	mkdir -p $(@)
