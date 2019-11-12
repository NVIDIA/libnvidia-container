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

DOCKER_TARGETS = ubuntu18.04 ubuntu16.04 debian10 debian9 centos7 amazonlinux1 amazonlinux2 opensuse-leap15.1

docker: SHELL:=/bin/bash
docker: $(DOCKER_TARGETS)

$(DOCKER_TARGETS): # Added to explicity define possible targets for bash completion

ubuntu%: ARCH := amd64
ubuntu%:
	$(DOCKER) build --build-arg VERSION_ID=$* \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/ubuntu:$* -f $(MAKE_DIR)/Dockerfile.ubuntu .
	$(MKDIR) -p $(DIST_DIR)/ubuntu$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION nvidia/$(LIB_NAME)/ubuntu:$*
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/ubuntu$*/$(ARCH)
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian%: ARCH := amd64
debian%:
	$(DOCKER) build --build-arg VERSION_ID=$* \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/debian:$* -f $(MAKE_DIR)/Dockerfile.debian .
	$(MKDIR) -p $(DIST_DIR)/debian$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION nvidia/$(LIB_NAME)/debian:$*
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/debian$*/$(ARCH)
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

centos%: ARCH := x86_64
centos%:
	$(DOCKER) build --build-arg VERSION_ID=$* \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/centos:$* -f $(MAKE_DIR)/Dockerfile.centos .
	$(MKDIR) -p $(DIST_DIR)/centos$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION nvidia/$(LIB_NAME)/centos:$*
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/centos$*/$(ARCH)
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amazonlinux%: ARCH := x86_64
amazonlinux%:
	$(DOCKER) build --build-arg VERSION_ID=$* \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/amazonlinux:$* -f $(MAKE_DIR)/Dockerfile.amazonlinux .
	$(MKDIR) -p $(DIST_DIR)/amazonlinux$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION nvidia/$(LIB_NAME)/amazonlinux:$*
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/amazonlinux$*/$(ARCH)
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid


opensuse-leap%: ARCH := x86_64
opensuse-leap%:
	$(DOCKER) build --build-arg VERSION_ID=$* \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/opensuse-leap:$* -f $(MAKE_DIR)/Dockerfile.opensuse-leap .
	$(MKDIR) -p $(DIST_DIR)/opensuse-leap$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION nvidia/$(LIB_NAME)/opensuse-leap:$*
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/opensuse-leap$*/$(ARCH)
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid
