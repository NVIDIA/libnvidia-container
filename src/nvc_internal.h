/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_NVC_INTERNAL_H
#define HEADER_NVC_INTERNAL_H

#include <sys/capability.h>
#include <sys/types.h>

#include <paths.h>
#include <stdbool.h>
#include <stdint.h>

#pragma GCC visibility push(default)
#include "nvc.h"
#pragma GCC visibility pop

#include "common.h"
#include "driver.h"
#include "error.h"
#include "ldcache.h"
#include "utils.h"
#include "dxcore.h"

#define SONAME_LIBCUDA  "libcuda.so.1"
#define SONAME_LIBNVML  "libnvidia-ml.so.1"
#define SONAME_LIBNVCGO "libnvidia-container-go.so.1"

#define NV_DEVICE_MAJOR          195
#define NV_CTL_DEVICE_MINOR      255
#define NV_MODESET_DEVICE_MINOR  254
#define NV_DEVICE_PATH           _PATH_DEV "nvidia%d"
#define NV_CTL_DEVICE_PATH       _PATH_DEV "nvidiactl"
#define NV_UVM_DEVICE_PATH       _PATH_DEV "nvidia-uvm"
#define NV_UVM_TOOLS_DEVICE_PATH _PATH_DEV "nvidia-uvm-tools"
#define NV_MODESET_DEVICE_PATH   _PATH_DEV "nvidia-modeset"
#define NV_CAPS_DEVICE_DIR       _PATH_DEV "nvidia-caps"
#define NV_CAPS_DEVICE_PATH      NV_CAPS_DEVICE_DIR "/nvidia-cap%d"
#define NV_CAPS_IMEX_DEVICE_DIR  _PATH_DEV "nvidia-caps-imex-channels"
#define NV_CAPS_IMEX_DEVICE_PATH NV_CAPS_IMEX_DEVICE_DIR "/channel%d"
#define NV_PERSISTENCED_SOCKET   _PATH_VARRUN "nvidia-persistenced/socket"
#define NV_FABRICMANAGER_SOCKET  _PATH_VARRUN "nvidia-fabricmanager/socket"
#define NV_MPS_PIPE_DIR          _PATH_TMP "nvidia-mps"
#define NV_PROC_DRIVER           "/proc/driver/nvidia"
#define NV_CAPS_PROC_DRIVER      "/proc/driver/nvidia-caps"
#define NV_UVM_PROC_DRIVER       "/proc/driver/nvidia-uvm"
#define NV_APP_PROFILE_DIR       "/etc/nvidia/nvidia-application-profiles-rc.d"
#define NV_CAPS_MIG_MINORS_PATH  NV_CAPS_PROC_DRIVER "/mig-minors"

#define NV_PROC_DRIVER_CAPS    NV_PROC_DRIVER "/capabilities"
#define NV_MIG_CAPS_PATH       NV_PROC_DRIVER_CAPS "/mig"
#define NV_GPU_CAPS_PATH       NV_PROC_DRIVER_CAPS "/gpu%d"
#define NV_GPU_MIG_CAPS_PATH   NV_GPU_CAPS_PATH "/mig"
#define NV_GPU_INST_CAPS_PATH  NV_GPU_MIG_CAPS_PATH "/gi%d"
#define NV_COMP_INST_CAPS_PATH NV_GPU_INST_CAPS_PATH "/ci%d"
#define NV_MIG_ACCESS_FILE     "access"
#define NV_MIG_CONFIG_FILE     "config"
#define NV_MIG_MONITOR_FILE    "monitor"

#define CUDA_RUNTIME_DIR         "/usr/local/cuda"

#define NV_FIRMWARE_PATH "/lib/firmware/nvidia/%s"
#define NV_FIRMWARE_GLOB "gsp*.bin"

#define MSFT_DXG_DEVICE_PATH     _PATH_DEV "dxg"

struct nvc_context {
        bool initialized;
        struct error err;
        struct nvc_config cfg;
        int mnt_ns;
        bool no_pivot;
        struct dxcore_context dxcore;
};

struct nvc_container {
        int32_t flags;
        struct nvc_container_config cfg;
        uid_t uid;
        gid_t gid;
        char *mnt_ns;
        int dev_cg_version;
        char *dev_cg;
        char **libs;
        size_t nlibs;
};

enum {
        NVC_INIT,
        NVC_INIT_KMODS,
        NVC_SHUTDOWN,
        NVC_CONTAINER,
        NVC_INFO,
        NVC_MOUNT,
        NVC_LDCACHE,
};

static const cap_value_t pcaps[] = {
        CAP_CHOWN,           /* kmods */
        CAP_DAC_OVERRIDE,    /* rhel userns, cgroups */
        CAP_DAC_READ_SEARCH, /* userns */
        CAP_FOWNER,          /* kmods */
        CAP_KILL,            /* privsep */
        CAP_MKNOD,           /* kmods */
        CAP_NET_ADMIN,       /* bpf_prog_query */
        CAP_SETGID,          /* privsep, userns */
        CAP_SETPCAP,         /* bounds, userns */
        CAP_SETUID,          /* privsep, userns */
        CAP_SYS_ADMIN,       /* setns, mount */
        CAP_SYS_CHROOT,      /* setns, chroot */
        CAP_SYS_PTRACE,      /* procns */
};

static const cap_value_t ecaps[][nitems(pcaps) + 1] = {
        [NVC_INIT]       = {CAP_KILL, CAP_SETUID, CAP_SETGID, CAP_SYS_CHROOT, -1},

        [NVC_INIT_KMODS] = {CAP_KILL,  CAP_SETUID, CAP_SETGID, CAP_SYS_CHROOT,
                            CAP_CHOWN, CAP_FOWNER, CAP_MKNOD, CAP_SETPCAP, -1},

        [NVC_SHUTDOWN]   = {CAP_KILL, -1},

        [NVC_CONTAINER]  = {CAP_KILL, CAP_DAC_READ_SEARCH, CAP_SYS_PTRACE, -1},

        [NVC_INFO]       = {CAP_KILL, -1},

        [NVC_MOUNT]      = {CAP_KILL, CAP_NET_ADMIN, CAP_SETUID, CAP_SETGID, CAP_SYS_CHROOT,
                            CAP_SYS_ADMIN, CAP_DAC_READ_SEARCH, CAP_SYS_PTRACE, CAP_DAC_OVERRIDE, -1},

        [NVC_LDCACHE]    = {CAP_KILL, CAP_SETUID, CAP_SETGID, CAP_SYS_CHROOT,
                            CAP_SYS_ADMIN, CAP_DAC_READ_SEARCH, CAP_SYS_PTRACE, CAP_SETPCAP, -1},
};

static const cap_value_t bcaps[] = {
        CAP_DAC_OVERRIDE,
        CAP_SYS_MODULE,
};

static inline size_t
ecaps_size(int idx)
{
        size_t i;

        for (i = 0; i < nitems(*ecaps); ++i) {
            if (ecaps[idx][i] == -1)
                break;
        }
        return (i);
}

static inline int
validate_context(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (-1);
        if (!ctx->initialized) {
                error_setx(&ctx->err, "context uninitialized");
                return (-1);
        }
        return (0);
}

static inline int
validate_args(struct nvc_context *ctx, bool predicate)
{
        if (!predicate) {
                error_setx(&ctx->err, "invalid argument");
                return (-1);
        }
        return (0);
}

/* Prototypes from nvc.c */
void nvc_entrypoint(void);

/* Prototypes from nvc_info.c */
bool match_binary_flags(const char *, int32_t);
bool match_library_flags(const char *, int32_t);
int find_device_node(struct error *, const char *, const char *, struct nvc_device_node *);

#endif /* HEADER_NVC_INTERNAL_H */
