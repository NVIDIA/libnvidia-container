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

#ifndef HEADER_LIBNVC_H
#define HEADER_LIBNVC_H

#include "nvc.h"

extern struct libnvc libnvc;

#define libnvc_entry(name) \
        __typeof__(nvc_##name) *name

struct libnvc {
        libnvc_entry(config_free);
        libnvc_entry(config_new);
        libnvc_entry(container_config_free);
        libnvc_entry(container_config_new);
        libnvc_entry(container_free);
        libnvc_entry(container_new);
        libnvc_entry(context_free);
        libnvc_entry(context_new);
        libnvc_entry(device_info_free);
        libnvc_entry(device_info_new);
        libnvc_entry(device_mount);
        libnvc_entry(driver_info_free);
        libnvc_entry(driver_info_new);
        libnvc_entry(symlink_libraries);
        libnvc_entry(driver_mount);
        libnvc_entry(error);
        libnvc_entry(init);
        libnvc_entry(ldcache_update);
        libnvc_entry(shutdown);
        libnvc_entry(version);
        libnvc_entry(nvcaps_style);
        libnvc_entry(nvcaps_device_from_proc_path);
        libnvc_entry(mig_device_access_caps_mount);
        libnvc_entry(mig_config_global_caps_mount);
        libnvc_entry(mig_monitor_global_caps_mount);
        libnvc_entry(device_mig_caps_mount);
};

int load_libnvc(void);

#endif /* HEADER_LIBNVC_H */
