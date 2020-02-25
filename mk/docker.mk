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

DOCKER_TARGETS = ubuntu18.04-amd64 ubuntu16.04-amd64 debian10-amd64 debian9-amd64 centos7-x86_64 amazonlinux2-x86_64 amazonlinux1-x86_64 opensuse-leap15.1-x86_64 sle15.1-x86_64 ubuntu16.04-ppc64le ubuntu18.04-ppc64le centos7-ppc64le

docker: SHELL:=/bin/bash
docker: $(DOCKER_TARGETS)

$(DOCKER_TARGETS): # Added to explicity define possible targets for bash completion

ubuntu18.04-%:
	$(DOCKER) build --build-arg VERSION_ID="18.04" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/ubuntu18.04-$*" -f $(MAKE_DIR)/$*/Dockerfile.ubuntu .
	$(MKDIR) -p $(DIST_DIR)/ubuntu18.04/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/ubuntu18.04-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/ubuntu18.04/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

ubuntu16.04-%:
	$(DOCKER) build --build-arg VERSION_ID="16.04" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/ubuntu16.04-$*" -f $(MAKE_DIR)/$*/Dockerfile.ubuntu .
	$(MKDIR) -p $(DIST_DIR)/ubuntu16.04/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/ubuntu16.04-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/ubuntu16.04/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian10-%:
	$(DOCKER) build --build-arg VERSION_ID="10" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/debian10-$*" -f $(MAKE_DIR)/$*/Dockerfile.debian .
	$(MKDIR) -p $(DIST_DIR)/debian10/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/debian10-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/debian10/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian9-%:
	$(DOCKER) build --build-arg VERSION_ID="9" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/debian9-$*" -f $(MAKE_DIR)/$*/Dockerfile.debian .
	$(MKDIR) -p $(DIST_DIR)/debian9/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/debian9-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/debian9/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

centos7-%:
	$(DOCKER) build --build-arg VERSION_ID="7" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/centos7-$*" -f $(MAKE_DIR)/$*/Dockerfile.centos .
	$(MKDIR) -p $(DIST_DIR)/centos7/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/centos7-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/centos7/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amazonlinux1-%:
	$(DOCKER) build --build-arg VERSION_ID="1" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/amazonlinux1-$*" -f $(MAKE_DIR)/$*/Dockerfile.amazonlinux .
	$(MKDIR) -p $(DIST_DIR)/amazonlinux1/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/amazonlinux1-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/amazonlinux1/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amazonlinux2-%:
	$(DOCKER) build --build-arg VERSION_ID="2" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/amazonlinux2-$*" -f $(MAKE_DIR)/$*/Dockerfile.amazonlinux .
	$(MKDIR) -p $(DIST_DIR)/amazonlinux2/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/amazonlinux2-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/amazonlinux2/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

opensuse-leap15.1-%:
	$(DOCKER) build --build-arg VERSION_ID="15.1" \
                    --build-arg WITH_LIBELF="$(WITH_LIBELF)" \
                    --build-arg WITH_TIRPC="$(WITH_TIRPC)" \
                    --build-arg WITH_SECCOMP="$(WITH_SECCOMP)" \
                    -t "nvidia/$(LIB_NAME)/opensuse-leap15.1-$*" -f $(MAKE_DIR)/$*/Dockerfile.opensuse-leap .
	$(MKDIR) -p $(DIST_DIR)/opensuse-leap15.1/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/opensuse-leap15.1-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/opensuse-leap15.1/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

sle15.1-%:
	$(DOCKER) build --build-arg VERSION_ID="15.1" \
                    --build-arg WITH_LIBELF=$(WITH_LIBELF) \
                    --build-arg WITH_TIRPC=$(WITH_TIRPC) \
                    --build-arg WITH_SECCOMP=$(WITH_SECCOMP) \
                    -t nvidia/$(LIB_NAME)/sle15:15.1-$* -f $(MAKE_DIR)/$*/Dockerfile.sle15 .
	$(MKDIR) -p $(DIST_DIR)/sle15.1/$*
	$(DOCKER) run --cidfile $@.cid -e DISTRIB -e SECTION "nvidia/$(LIB_NAME)/sle15:15.1-$*"
	$(DOCKER) cp $$(cat $@.cid):/mnt/. $(DIST_DIR)/sle15.1/$*
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid
