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

GIT_TAG ?= $(patsubst v%,%,$(shell git describe --tags 2>/dev/null))
GIT_COMMIT ?= $(shell git describe --match="" --dirty --long --always --abbrev=40 2> /dev/null || echo "")

LIB_NAME := libnvidia-container
LIB_VERSION ?= $(word 1,$(subst -, ,$(GIT_TAG)))
LIB_TAG ?= $(subst -,+,$(patsubst $(LIB_VERSION)-%,%,$(GIT_TAG)))

VERSION_PARTS := $(subst ., ,$(LIB_VERSION))
MAJOR := $(word 1,$(VERSION_PARTS))
MINOR := $(word 2,$(VERSION_PARTS))
PATCH := $(word 3,$(VERSION_PARTS))

VERSION := $(LIB_VERSION)
TAG := $(LIB_TAG)
VERSION_STRING := $(LIB_VERSION)$(if $(TAG),-$(TAG),)
