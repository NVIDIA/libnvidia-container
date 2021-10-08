/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HEADER_CGROUP_H
#define HEADER_CGROUP_H

#include <sys/types.h>

#include "error.h"
#include "nvc_internal.h"

char *find_device_cgroup_path(struct error *, const struct nvc_container *);
int  setup_device_cgroup(struct error *, const struct nvc_container *, dev_t);

#endif /* HEADER_CGROUP_H */
