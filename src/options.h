/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_OPTIONS_H
#define HEADER_OPTIONS_H

#include <stdint.h>

#include "error.h"

struct option {
        const char *name;
        int32_t value;
};

/* Library options */
enum {
        OPT_LOAD_KMODS              = 1 << 0,
        OPT_NO_CREATE_IMEX_CHANNELS = 1 << 1,
};

static const struct option library_opts[] = {
        {"load-kmods", OPT_LOAD_KMODS},
        {"no-create-imex-channels", OPT_NO_CREATE_IMEX_CHANNELS}
};

static const char * const default_library_opts = "";

/* Driver options */
enum {
        OPT_NO_GLVND         = 1 << 0,
        OPT_NO_UVM           = 1 << 1,
        OPT_NO_MODESET       = 1 << 2,
        OPT_NO_MPS           = 1 << 3,
        OPT_NO_PERSISTENCED  = 1 << 4,
        OPT_NO_FABRICMANAGER = 1 << 5,
        OPT_NO_GSP_FIRMWARE  = 1 << 6,
};

static const struct option driver_opts[] = {
        {"no-glvnd", OPT_NO_GLVND},
        {"no-uvm", OPT_NO_UVM},
        {"no-modeset", OPT_NO_MODESET},
        {"no-mps", OPT_NO_MPS},
        {"no-persistenced", OPT_NO_PERSISTENCED},
        {"no-fabricmanager", OPT_NO_FABRICMANAGER},
        {"no-gsp-firmware", OPT_NO_GSP_FIRMWARE},
};

static const char * const default_driver_opts = "";

/* Device options */
/* static const struct option device_opts[] = {}; */

static const char * const default_device_opts = "";

/* Container options */
enum {
        OPT_SUPERVISED    = 1 << 0,
        OPT_STANDALONE    = 1 << 1,
        OPT_NO_CGROUPS    = 1 << 2,
        OPT_NO_DEVBIND    = 1 << 3,
        OPT_UTILITY_LIBS  = 1 << 5,
        OPT_COMPUTE_LIBS  = 1 << 6,
        OPT_NGX_LIBS      = 1 << 7,
        OPT_VIDEO_LIBS    = 1 << 8,
        OPT_GRAPHICS_LIBS = 1 << 9,
        OPT_DISPLAY       = 1 << 10,
        OPT_UTILITY_BINS  = 1 << 11,
        OPT_COMPUTE_BINS  = 1 << 12,
#if defined(__powerpc64__) /* ppc64le doesn't support compat32. */
        OPT_COMPAT32      = 1 << 0,
#else
        OPT_COMPAT32      = 1 << 13,
#endif /* defined(__powerpc64__) */
        /* OPT_CUDA_COMPAT_MODE_DISABLED replaced OPT_NO_CNTLIBS. */
        OPT_CUDA_COMPAT_MODE_DISABLED = 1 << 14,
        OPT_CUDA_COMPAT_MODE_LDCONFIG = 1 << 15,
        OPT_CUDA_COMPAT_MODE_MOUNT    = 1 << 16,
};

static const struct option container_opts[] = {
        {"supervised", OPT_SUPERVISED},
        {"standalone", OPT_STANDALONE},
        {"no-cgroups", OPT_NO_CGROUPS},
        {"no-devbind", OPT_NO_DEVBIND},
        {"utility", OPT_UTILITY_BINS|OPT_UTILITY_LIBS},
        {"compute", OPT_COMPUTE_BINS|OPT_COMPUTE_LIBS},
        {"video", OPT_VIDEO_LIBS|OPT_COMPUTE_LIBS},
        {"graphics", OPT_GRAPHICS_LIBS},
        {"display", OPT_DISPLAY|OPT_GRAPHICS_LIBS},
        {"ngx", OPT_NGX_LIBS},
        {"compat32", OPT_COMPAT32},
        {"cuda-compat-mode=disabled", OPT_CUDA_COMPAT_MODE_DISABLED},
        {"cuda-compat-mode=mount", OPT_CUDA_COMPAT_MODE_MOUNT},
        {"cuda-compat-mode=ldconfig", OPT_CUDA_COMPAT_MODE_LDCONFIG},
};

static const char * const default_container_opts = "standalone no-cgroups no-devbind utility";

int32_t options_parse(struct error *, const char *, const struct option *, size_t);

#endif /* HEADER_OPTIONS_H */
