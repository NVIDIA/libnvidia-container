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

#include <sys/types.h>

#include <assert.h>
#include <stdbool.h>

#include "cli/libnvc.h"

struct libnvc libnvc = {0};

int
load_libnvc(void)
{
        #define load_libnvc_func(func) \
            libnvc.func = nvc_##func

        load_libnvc_func(config_free);
        load_libnvc_func(config_new);
        load_libnvc_func(container_config_free);
        load_libnvc_func(container_config_new);
        load_libnvc_func(container_free);
        load_libnvc_func(container_new);
        load_libnvc_func(context_free);
        load_libnvc_func(context_new);
        load_libnvc_func(device_info_free);
        load_libnvc_func(device_info_new);
        load_libnvc_func(device_mount);
        load_libnvc_func(driver_info_free);
        load_libnvc_func(driver_info_new);
        load_libnvc_func(driver_mount);
        load_libnvc_func(error);
        load_libnvc_func(init);
        load_libnvc_func(ldcache_update);
        load_libnvc_func(shutdown);
        load_libnvc_func(version);
        load_libnvc_func(nvcaps_style);
        load_libnvc_func(nvcaps_device_from_proc_path);
        load_libnvc_func(mig_device_access_caps_mount);
        load_libnvc_func(mig_config_global_caps_mount);
        load_libnvc_func(mig_monitor_global_caps_mount);
        load_libnvc_func(device_mig_caps_mount);

        return (0);
}
