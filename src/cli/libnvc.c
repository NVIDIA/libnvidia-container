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
#include <dlfcn.h>
#include <err.h>
#include <stdbool.h>
#include <stddef.h>

#include "cli/libnvc.h"

#include "utils.h"

struct libnvc libnvc = {0};

static const char *tegra_release_file = "/etc/nv_tegra_release";
static const char *tegra_family_file = "/sys/devices/soc0/family";
static const char *libnvc_v0_soname= "libnvidia-container.so.0";
static const char *libnvml_soname= "libnvidia-ml.so.1";

static struct libnvc libnvc_v0_wrapped = {0};

struct libnvc_v0_wrapper_driver_info {
        struct nvc_driver_info v1;
        void *v0;
};

struct libnvc_v0_wrapper_device_info {
        struct nvc_device_info v1;
        void *v0;
};

static int load_libnvc_v0(void);
static int load_libnvc_v1(void);

static bool is_tegra(void);
static bool nvml_available(void);

static void libnvc_v0_wrapper_device_info_free(struct nvc_device_info *);
static struct nvc_device_info *libnvc_v0_wrapper_device_info_new(struct nvc_context *, const char *);
static int libnvc_v0_wrapper_device_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_device *);
static void libnvc_v0_wrapper_driver_info_free(struct nvc_driver_info *);
static struct nvc_driver_info *libnvc_v0_wrapper_driver_info_new(struct nvc_context *, const char *);
static int libnvc_v0_wrapper_driver_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_driver_info *);

int
load_libnvc(void)
{
        if (is_tegra() && !nvml_available())
                return load_libnvc_v0();
        return load_libnvc_v1();
}

static int
load_libnvc_v0(void)
{
        #define funcs_entry(name) \
            {(const void**)&libnvc.name, "nvc_" #name}

        #define wrap_libnvc_func(func) \
            libnvc_v0_wrapped.func = libnvc.func; \
            libnvc.func = libnvc_v0_wrapper_##func

        struct {
                const void **f;
                const char *symbol;
        } funcs[] = {
                funcs_entry(config_free),
                funcs_entry(config_new),
                funcs_entry(container_config_free),
                funcs_entry(container_config_new),
                funcs_entry(container_free),
                funcs_entry(container_new),
                funcs_entry(context_free),
                funcs_entry(context_new),
                funcs_entry(device_info_free),
                funcs_entry(device_info_new),
                funcs_entry(device_mount),
                funcs_entry(driver_info_free),
                funcs_entry(driver_info_new),
                funcs_entry(driver_mount),
                funcs_entry(error),
                funcs_entry(init),
                funcs_entry(ldcache_update),
                funcs_entry(shutdown),
                funcs_entry(version),
        };

        const size_t len_libnvc = offsetof(struct libnvc, nvcaps_style)/sizeof(void*);
        const size_t len_funcs = sizeof(funcs)/sizeof(*funcs);
        static_assert(len_funcs == len_libnvc, "len(libnvc) != len(funcs)");

        void *handle = dlopen(libnvc_v0_soname, RTLD_NOW);
        if (!handle) {
                warnx("Error: %s", dlerror());
                return (-1);
        }

        for (size_t i = 0; i < len_libnvc; i++) {
                *funcs[i].f = dlsym(handle, funcs[i].symbol);
                if (!*funcs[i].f) {
                    warnx("Error: %s", dlerror());
                    dlclose(handle);
                    return (-1);
                }
        }

        wrap_libnvc_func(device_info_free);
        wrap_libnvc_func(device_info_new);
        wrap_libnvc_func(device_mount);
        wrap_libnvc_func(driver_info_free);
        wrap_libnvc_func(driver_info_new);
        wrap_libnvc_func(driver_mount);

        return (0);
}

static int
load_libnvc_v1(void)
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

static bool
is_tegra(void)
{
        static int is_tegra = -1;
        int release_exists = false;
        char *family_contents = NULL;
        struct error err = {};

        if (is_tegra != -1)
            return is_tegra;

        release_exists = file_exists(&err, tegra_release_file);
        if (release_exists < 0) {
                log_errf("%s", err.msg);
                goto error;
        }
        if (release_exists) {
                is_tegra = true;
                goto success;
        }

        if (file_read_text(&err, tegra_family_file, &family_contents) < 0) {
                log_errf("%s", err.msg);
                goto error;
        }
        if (!strncasecmp(family_contents, "tegra", 5)) {
                is_tegra = true;
                goto success;
        }

        is_tegra = false;

success:
        free(family_contents);
        return (is_tegra);
error:
        error_reset(&err);
        return false;
}

static bool
nvml_available(void)
{
        static int available = -1;
        if (available != -1)
            return available;

        available = false;
        void *handle = dlopen(libnvml_soname, RTLD_LAZY);
        if (handle) {
            dlclose(handle);
            available = true;
        }

        return (available);
}

static void
libnvc_v0_wrapper_device_info_free(struct nvc_device_info *info_)
{
        if (info_ == NULL)
                return;

        struct libnvc_v0_wrapper_device_info *info = (void*)info_;
        libnvc_v0_wrapped.device_info_free(info->v0);
        free(info->v1.gpus);
        free(info);
}

static struct nvc_device_info *
libnvc_v0_wrapper_device_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_device_info *info_;
        info_ = libnvc_v0_wrapped.device_info_new(ctx, opts);
        if (info_ == NULL)
                return NULL;

        struct libnvc_v0_wrapper_device_info *info;
        info = calloc(1, sizeof(*info));
        if (info == NULL) {
                libnvc_v0_wrapped.device_info_free(info_);
                return NULL;
        }

        info->v0 = info_;
        info->v1 = *info_;
        info->v1.gpus = calloc(info->v1.ngpus, sizeof(*info->v1.gpus));
        if (info->v1.gpus == NULL) {
                libnvc_v0_wrapper_device_info_free((void*)info);
                return NULL;
        }
        for (size_t i = 0; i < info->v1.ngpus; i++)
                memcpy(&info->v1.gpus[i], &info_->gpus[i], offsetof(struct nvc_device, mig_capable));

        return &info->v1;
}

static int
libnvc_v0_wrapper_device_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        // There are no device mounts to do in the v0 API.
        // All mounts are driver mounts.
        (void)ctx; (void)cnt; (void)dev;
        return 0;
}

static void
libnvc_v0_wrapper_driver_info_free(struct nvc_driver_info *info_)
{
        if (info_ == NULL)
                return;

        struct libnvc_v0_wrapper_driver_info *info = (void*)info_;
        libnvc_v0_wrapped.driver_info_free(info->v0);
        free(info);
}

static struct nvc_driver_info *
libnvc_v0_wrapper_driver_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_driver_info *info_;
        info_ = libnvc_v0_wrapped.driver_info_new(ctx, opts);
        if (info_ == NULL)
                return NULL;

        struct libnvc_v0_wrapper_driver_info *info;
        info = calloc(1, sizeof(*info));
        if (info == NULL) {
                libnvc_v0_wrapped.driver_info_free(info_);
                return NULL;
        }

        info->v0 = info_;
        memcpy(&info->v1, info_, offsetof(struct nvc_driver_info, devs));

        return &info->v1;
}

static int
libnvc_v0_wrapper_driver_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_driver_info *info_)
{
        struct libnvc_v0_wrapper_driver_info *info = (void*)info_;
        return libnvc_v0_wrapped.driver_mount(ctx, cnt, info->v0);
}
