/*
 * Copyright (c) NVIDIA CORPORATION.  All rights reserved.
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

#ifndef HEADER_NVC_H
#define HEADER_NVC_H

#include <sys/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NVC_MAJOR   1
#define NVC_MINOR   14
#define NVC_PATCH   0

// Specify the release tag.
// For stable releases, this should be defined as empty.
// For release candidates, this should be defined with the format "rc.1"
// The version string should also be updated accordingly, using a - separator where applicable.
#define NVC_TAG "rc.2"
#define NVC_VERSION "1.14.0-rc.2"

#define NVC_ARG_MAX 256

#define NVC_NVCAPS_STYLE_NONE 0
#define NVC_NVCAPS_STYLE_PROC 1
#define NVC_NVCAPS_STYLE_DEV  2

struct nvc_context;
struct nvc_container;

struct nvc_version {
        unsigned int major;
        unsigned int minor;
        unsigned int patch;
        const char *string;
};

struct nvc_config {
        char *root;
        char *ldcache;
        uid_t uid;
        gid_t gid;
};

struct nvc_device_node {
        char *path;
        dev_t id;
};

struct nvc_driver_info {
        char *nvrm_version;
        char *cuda_version;
        char **bins;
        size_t nbins;
        char **libs;
        size_t nlibs;
        char **libs32;
        size_t nlibs32;
        char **ipcs;
        size_t nipcs;
        struct nvc_device_node *devs;
        size_t ndevs;
        char **firmwares;
        size_t nfirmwares;
};

struct nvc_mig_device {
        struct nvc_device *parent;
        char *uuid;
        unsigned int gi;
        unsigned int ci;
        char *gi_caps_path;
        char *ci_caps_path;
};

struct nvc_mig_device_info {
        struct nvc_mig_device *devices;
        size_t ndevices;
};

struct nvc_device {
        char *model;
        char *uuid;
        char *busid;
        char *arch;
        char *brand;
        struct nvc_device_node node;
        bool mig_capable;
        char *mig_caps_path;
        struct nvc_mig_device_info mig_devices;
};

struct nvc_device_info {
        struct nvc_device *gpus;
        size_t ngpus;
};

struct nvc_container_config {
        pid_t pid;
        char *rootfs;
        char *bins_dir;
        char *libs_dir;
        char *libs32_dir;
        char *cudart_dir;
        char *ldconfig;
};

const struct nvc_version *nvc_version(void);

struct nvc_context *nvc_context_new(void);
void nvc_context_free(struct nvc_context *);

struct nvc_config *nvc_config_new(void);
void nvc_config_free(struct nvc_config *);

int nvc_init(struct nvc_context *, const struct nvc_config *, const char *);
int nvc_shutdown(struct nvc_context *);

struct nvc_container_config *nvc_container_config_new(pid_t, const char *);
void nvc_container_config_free(struct nvc_container_config *);

struct nvc_container *nvc_container_new(struct nvc_context *, const struct nvc_container_config *, const char *);
void nvc_container_free(struct nvc_container *);

struct nvc_driver_info *nvc_driver_info_new(struct nvc_context *, const char *);
void nvc_driver_info_free(struct nvc_driver_info *);

struct nvc_device_info *nvc_device_info_new(struct nvc_context *, const char *);
void nvc_device_info_free(struct nvc_device_info *);

int nvc_nvcaps_style(void);

int nvc_nvcaps_device_from_proc_path(struct nvc_context *, const char *, struct nvc_device_node *);

int nvc_symlink_libraries(struct nvc_context *, const struct nvc_container *, const struct nvc_driver_info *);

int nvc_driver_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_driver_info *);

int nvc_device_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_device *);

int nvc_mig_device_access_caps_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_mig_device *);

int nvc_mig_config_global_caps_mount(struct nvc_context *, const struct nvc_container *);

int nvc_mig_monitor_global_caps_mount(struct nvc_context *, const struct nvc_container *);

int nvc_device_mig_caps_mount(struct nvc_context *, const struct nvc_container *, const struct nvc_device *);

int nvc_ldcache_update(struct nvc_context *, const struct nvc_container *);

const char *nvc_error(struct nvc_context *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEADER_NVC_H */
