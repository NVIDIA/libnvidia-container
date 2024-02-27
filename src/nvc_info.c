/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>

#include <errno.h>
#include <limits.h>
#include <nvidia-modprobe-utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "driver.h"
#include "elftool.h"
#include "error.h"
#include "ldcache.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

#define MAX_BINS (nitems(utility_bins) + \
                  nitems(compute_bins))
#define MAX_LIBS (nitems(dxcore_libs) + \
                  nitems(ngx_libs) + \
                  nitems(utility_libs) + \
                  nitems(compute_libs) + \
                  nitems(video_libs) + \
                  nitems(graphics_libs) + \
                  nitems(graphics_libs_glvnd) + \
                  nitems(graphics_libs_compat))

static int select_libraries(struct error *, void *, const char *, const char *, const char *);
static int select_wsl_libraries(struct error *, void *, const char *, const char *, const char *);
static int find_library_paths(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, const char *, const char * const [], size_t);
static int find_binary_paths(struct error *, struct dxcore_context*, struct nvc_driver_info *, const char *, const char * const [], size_t);
static int find_path(struct error *, const char *, const char *, const char *, char **);
static int lookup_paths(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, int32_t, const char *);
static int lookup_libraries(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, int32_t, const char *);
static int lookup_binaries(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, int32_t);
static int lookup_firmwares(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, int32_t);
static int lookup_devices(struct error *, struct dxcore_context *, struct nvc_driver_info *, const char *, int32_t);
static int lookup_ipcs(struct error *, struct nvc_driver_info *, const char *, int32_t);
static int fill_mig_device_info(struct nvc_context *, bool mig_enabled, struct driver_device *, struct nvc_device *);
static void clear_mig_device_info(struct nvc_mig_device_info *);

/*
 * Display libraries are not needed.
 *
 * "libnvidia-gtk2.so" // GTK2 (used by nvidia-settings)
 * "libnvidia-gtk3.so" // GTK3 (used by nvidia-settings)
 * "libnvidia-wfb.so"  // Wrapped software rendering module for X server
 * "nvidia_drv.so"     // Driver module for X server
 * "libglx.so"         // GLX extension module for X server
 */

static const char * const utility_bins[] = {
        "nvidia-smi",                       /* System management interface */
        "nvidia-debugdump",                 /* GPU coredump utility */
        "nvidia-persistenced",              /* Persistence mode utility */
        "nv-fabricmanager",                 /* NVSwitch fabrimanager utility */
        //"nvidia-modprobe",                /* Kernel module loader */
        //"nvidia-settings",                /* X server settings */
        //"nvidia-xconfig",                 /* X xorg.conf editor */
};

static const char * const compute_bins[] = {
        "nvidia-cuda-mps-control",          /* Multi process service CLI */
        "nvidia-cuda-mps-server",           /* Multi process service server */
};

static const char * const utility_libs[] = {
        "libnvidia-ml.so",                  /* Management library */
        "libnvidia-cfg.so",                 /* GPU configuration */
        "libnvidia-nscq.so",                /* Topology info for NVSwitches and GPUs */
};

static const char * const compute_libs[] = {
        "libcuda.so",                       /* CUDA driver library */
        "libcudadebugger.so",               /* CUDA Debugger Library */
        "libnvidia-opencl.so",              /* NVIDIA OpenCL ICD */
        "libnvidia-gpucomp.so",             /* Shared Compiler Library */
        "libnvidia-ptxjitcompiler.so",      /* PTX-SASS JIT compiler (used by libcuda) */
        "libnvidia-fatbinaryloader.so",     /* fatbin loader (used by libcuda) */
        "libnvidia-allocator.so",           /* NVIDIA allocator runtime library */
        "libnvidia-compiler.so",            /* NVVM-PTX compiler for OpenCL (used by libnvidia-opencl) */
        "libnvidia-pkcs11.so",              /* Encrypt/Decrypt library */
        "libnvidia-pkcs11-openssl3.so",     /* Encrypt/Decrypt library (OpenSSL 3 support) */
        "libnvidia-nvvm.so",                /* The NVVM Compiler library */
};

static const char * const video_libs[] = {
        "libvdpau_nvidia.so",               /* NVIDIA VDPAU ICD */
        "libnvidia-encode.so",              /* Video encoder */
        "libnvidia-opticalflow.so",         /* NVIDIA Opticalflow library */
        "libnvcuvid.so",                    /* Video decoder */
};

static const char * const graphics_libs[] = {
        //"libnvidia-egl-wayland.so",       /* EGL wayland platform extension (used by libEGL_nvidia) */
        "libnvidia-eglcore.so",             /* EGL core (used by libGLES*[_nvidia] and libEGL_nvidia) */
        "libnvidia-glcore.so",              /* OpenGL core (used by libGL or libGLX_nvidia) */
        "libnvidia-tls.so",                 /* Thread local storage (used by libGL or libGLX_nvidia) */
        "libnvidia-glsi.so",                /* OpenGL system interaction (used by libEGL_nvidia) */
        "libnvidia-fbc.so",                 /* Framebuffer capture */
        "libnvidia-ifr.so",                 /* OpenGL framebuffer capture */
        "libnvidia-rtcore.so",              /* Optix */
        "libnvoptix.so",                    /* Optix */
};

static const char * const graphics_libs_glvnd[] = {
        //"libGLX.so",                      /* GLX ICD loader */
        //"libOpenGL.so",                   /* OpenGL ICD loader */
        //"libGLdispatch.so",               /* OpenGL dispatch (used by libOpenGL, libEGL and libGLES*) */
        "libGLX_nvidia.so",                 /* OpenGL/GLX ICD */
        "libEGL_nvidia.so",                 /* EGL ICD */
        "libGLESv2_nvidia.so",              /* OpenGL ES v2 ICD */
        "libGLESv1_CM_nvidia.so",           /* OpenGL ES v1 common profile ICD */
        "libnvidia-glvkspirv.so",           /* SPIR-V Lib for Vulkan */
        "libnvidia-cbl.so",                 /* VK_NV_ray_tracing */
};

static const char * const graphics_libs_compat[] = {
        "libGL.so",                         /* OpenGL/GLX legacy _or_ compatibility wrapper (GLVND) */
        "libEGL.so",                        /* EGL legacy _or_ ICD loader (GLVND) */
        "libGLESv1_CM.so",                  /* OpenGL ES v1 common profile legacy _or_ ICD loader (GLVND) */
        "libGLESv2.so",                     /* OpenGL ES v2 legacy _or_ ICD loader (GLVND) */
};

static const char * const ngx_libs[] = {
        "libnvidia-ngx.so",                 /* NGX library */
};

static const char * const dxcore_libs[] = {
        "libdxcore.so",                     /* Core library for dxcore support */
};

static int
select_libraries(struct error *err, void *ptr, const char *root, const char *orig_path, const char *alt_path)
{
        char path[PATH_MAX];
        struct nvc_driver_info *info = ptr;
        struct elftool et;
        char *lib;
        int rv = true;

        if (path_join(err, path, root, alt_path) < 0)
                return (-1);
        elftool_init(&et, err);
        if (elftool_open(&et, path) < 0)
                return (-1);

        lib = basename(alt_path);
        if (str_has_prefix(lib, "libnvidia-tls.so")) {
                /* Only choose the TLS library using the new ABI (kernel 2.3.99). */
                if ((rv = elftool_has_abi(&et, (uint32_t[3]){0x02, 0x03, 0x63})) != true)
                        goto done;
        }
        /* Check the driver version. */
        if ((rv = str_has_suffix(lib, info->nvrm_version)) == false)
                goto done;
        if (str_array_match_prefix(lib, graphics_libs_compat, nitems(graphics_libs_compat))) {
                /* Only choose OpenGL/EGL libraries issued by NVIDIA. */
                if ((rv = elftool_has_dependency(&et, "libnvidia-glcore.so")) != false)
                        goto done;
                if ((rv = elftool_has_dependency(&et, "libnvidia-eglcore.so")) != false)
                        goto done;
        }

 done:
        if (rv)
                log_infof((orig_path == NULL) ? "%s %s" : "%s %s over %s", "selecting", alt_path, orig_path);
        else
                log_infof("skipping %s", alt_path);

        elftool_close(&et);
        return (rv);
}

static int
select_wsl_libraries(struct error *err, void *ptr, const char *root, const char *orig_path, const char *alt_path)
{
        int rv = true;

        // Unused parameters
        err = err;
        ptr = ptr;
        root = root;

        // Always prefer the lxss libraries
        if (orig_path && strstr(orig_path, "/wsl/lib/")) {
                rv = false;
                goto done;
        }

 done:
        if (rv)
                log_infof((orig_path == NULL) ? "%s %s" : "%s %s over %s", "selecting", alt_path, orig_path);
        else
                log_infof("skipping %s", alt_path);
        return (rv);
}

static int
find_library_paths(struct error *err, struct dxcore_context *dxcore, struct nvc_driver_info *info,
                   const char *root, const char *ldcache, const char * const libs[], size_t size)
{
        char path[PATH_MAX];
        struct ldcache ld;
        int rv = -1;

        ldcache_select_fn select_libraries_fn = dxcore->initialized ? select_wsl_libraries : select_libraries;

        if (path_resolve_full(err, path, root, ldcache) < 0)
                return (-1);
        ldcache_init(&ld, err, path);
        if (ldcache_open(&ld) < 0)
                return (-1);

        info->nlibs = size;
        info->libs = array_new(err, size);
        if (info->libs == NULL)
                goto fail;
        if (ldcache_resolve(&ld, LIB_ARCH, root, libs,
            info->libs, info->nlibs, select_libraries_fn, info) < 0)
                goto fail;

        info->nlibs32 = size;
        info->libs32 = array_new(err, size);
        if (info->libs32 == NULL)
                goto fail;
        if (ldcache_resolve(&ld, LIB32_ARCH, root, libs,
            info->libs32, info->nlibs32, select_libraries_fn, info) < 0)
                goto fail;
        rv = 0;

 fail:
        if (ldcache_close(&ld) < 0)
                return (-1);
        return (rv);
}

static int
find_binary_paths(struct error *err, struct dxcore_context* dxcore, struct nvc_driver_info* info,
                  const char *root, const char * const bins[], size_t size)
{
        char *env, *ptr;
        const char *dir;
        char tmp[PATH_MAX];
        char path[PATH_MAX];
        int rv = -1;

        if ((env = secure_getenv("PATH")) == NULL) {
                error_setx(err, "environment variable PATH not found");
                return (-1);
        }
        if ((env = ptr = xstrdup(err, env)) == NULL)
                return (-1);

        info->nbins = size;
        info->bins = array_new(err, size);
        if (info->bins == NULL)
                goto fail;

        // If we are on WSL we want to check if we have a copy of this
        // binary in our driver store first
        if (dxcore->initialized) {
                for (size_t i = 0; i < size; ++i) {
                        for (unsigned int adapterIndex = 0; adapterIndex < dxcore->adapterCount; adapterIndex++) {
                                if (path_join(NULL, tmp, dxcore->adapterList[adapterIndex].pDriverStorePath, bins[i]) < 0)
                                        continue;
                                if (path_resolve(NULL, path, root, tmp) < 0)
                                        continue;
                                if (file_exists_at(NULL, root, path) == true) {
                                        info->bins[i] = xstrdup(err, path);
                                        if (info->bins[i] == NULL)
                                                goto fail;
                                        log_infof("selecting %s", path);
                                }
                        }
                }
        }

        while ((dir = strsep(&ptr, ":")) != NULL) {
                if (*dir == '\0')
                        dir = ".";
                for (size_t i = 0; i < size; ++i) {
                        if (info->bins[i] != NULL)
                                continue;
                        if (path_join(NULL, tmp, dir, bins[i]) < 0)
                                continue;
                        if (path_resolve(NULL, path, root, tmp) < 0)
                                continue;
                        if (file_exists_at(NULL, root, path) == true) {
                                info->bins[i] = xstrdup(err, path);
                                if (info->bins[i] == NULL)
                                        goto fail;
                                log_infof("selecting %s", path);
                        }
                }
        }
        rv = 0;

 fail:
        free(env);
        return (rv);
}

int
find_device_node(struct error *err, const char *root, const char *dev, struct nvc_device_node *node)
{
        char path[PATH_MAX];
        struct stat s;

        if (path_resolve_full(err, path, root, dev) < 0)
                return (-1);
        if (xstat(err, path, &s) == 0) {
                *node = (struct nvc_device_node){(char *)dev, s.st_rdev};
                return (true);
        }
        if (err->code == ENOENT) {
                log_warnf("missing device %s", dev);
                return (false);
        }
        return (-1);
}

// find_path resolves a path relative to the specified root. If the path exists, the
// output buffer is populated with the resolved path not including the root. A `tag` parameter is
// provided to control logging output.
static int
find_path(struct error *err, const char *tag, const char *root, const char *target, char **buf)
{
        char path[PATH_MAX];
        int ret;

        if (path_resolve(err, path, root, target) < 0)
                return (-1);
        if ((ret = file_exists_at(err, root, path)) < 0)
                return (-1);
        if (ret) {
                log_infof("listing %s path %s", tag, path);
                if ((*buf = xstrdup(err, path)) == NULL) {
                        log_err("error creating output buffer");
                        return (-1);
                }
        } else {
                log_warnf("missing %s path %s", tag, target);
        }
        return (0);
}

static int
lookup_paths(struct error *err, struct dxcore_context *dxcore, struct nvc_driver_info *info, const char *root, int32_t flags, const char *ldcache)
{
        if (lookup_libraries(err, dxcore, info, root, flags, ldcache) < 0) {
                log_err("error looking up libraries");
                return (-1);
        }

        if (lookup_binaries(err, dxcore, info, root, flags) < 0) {
                log_err("error looking up binaries");
                return (-1);
        }

        if (lookup_firmwares(err, dxcore, info, root, flags) < 0) {
                log_err("error looking up additional paths");
                return (-1);
        }

        return (0);
}

static int
lookup_libraries(struct error *err, struct dxcore_context *dxcore, struct nvc_driver_info *info, const char *root, int32_t flags, const char *ldcache)
{
        const char *libs[MAX_LIBS];
        const char **ptr = libs;

        ptr = array_append(ptr, utility_libs, nitems(utility_libs));
        ptr = array_append(ptr, compute_libs, nitems(compute_libs));
        ptr = array_append(ptr, ngx_libs, nitems(ngx_libs));
        ptr = array_append(ptr, video_libs, nitems(video_libs));
        ptr = array_append(ptr, graphics_libs, nitems(graphics_libs));
        if (flags & OPT_NO_GLVND)
                ptr = array_append(ptr, graphics_libs_compat, nitems(graphics_libs_compat));
        else
                ptr = array_append(ptr, graphics_libs_glvnd, nitems(graphics_libs_glvnd));

        if (dxcore->initialized)
                ptr = array_append(ptr, dxcore_libs, nitems(dxcore_libs));

        if (find_library_paths(err, dxcore, info, root, ldcache, libs, (size_t)(ptr - libs)) < 0)
                return (-1);

        for (size_t i = 0; info->libs != NULL && i < info->nlibs; ++i) {
                if (info->libs[i] == NULL)
                        log_warnf("missing library %s", libs[i]);
        }
        for (size_t i = 0; info->libs32 != NULL && i < info->nlibs32; ++i) {
                if (info->libs32[i] == NULL)
                        log_warnf("missing compat32 library %s", libs[i]);
        }
        array_pack(info->libs, &info->nlibs);
        array_pack(info->libs32, &info->nlibs32);
        return (0);
}

static int
lookup_binaries(struct error *err, struct dxcore_context* dxcore, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        const char *bins[MAX_BINS];
        const char **ptr = bins;

        ptr = array_append(ptr, utility_bins, nitems(utility_bins));
        if (!(flags & OPT_NO_MPS))
                ptr = array_append(ptr, compute_bins, nitems(compute_bins));

        if (find_binary_paths(err, dxcore, info, root, bins, (size_t)(ptr - bins)) < 0)
                return (-1);

        for (size_t i = 0; info->bins != NULL && i < info->nbins; ++i) {
                if (info->bins[i] == NULL)
                        log_warnf("missing binary %s", bins[i]);
        }
        array_pack(info->bins, &info->nbins);
        return (0);
}

static int
lookup_firmwares(struct error *err, struct dxcore_context *dxcore, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        (void)flags;

        glob_t gl = {0};
        char glob_path[PATH_MAX];
        char *firmware_path = NULL;
        int rv = -1;

        if (dxcore->initialized) {
                log_info("skipping path lookup for dxcore");
                goto success;
        }

        // Construct the fully resolved NV_FIRMWARE_PATH_GLOB.
        if (xasprintf(err, &firmware_path, NV_FIRMWARE_PATH, info->nvrm_version) < 0) {
                log_errf("error constructing firmware path for %s", info->nvrm_version);
                goto fail;
        }
        if (path_resolve_full(err, glob_path, root, firmware_path) < 0) {
                log_errf("error resolving firmware path %s", firmware_path);
                goto fail;
        }
        if (path_append(err, glob_path, NV_FIRMWARE_GLOB) < 0) {
                log_errf("error appending glob to firmware path %s", firmware_path);
                goto fail;
        }

        // Walk each path matched in the fully resolved glob_path and
        // include the non-resolved path in our list of mounted files.
        if (xglob(err, glob_path, GLOB_ERR, NULL, &gl) < 0) {
                log_errf("error processing firmware path glob of %s", glob_path);
                goto fail;
        }

        if (gl.gl_pathc == 0) {
                log_warnf("missing firmware path %s", glob_path);
                goto success;
        }

        info->nfirmwares = gl.gl_pathc;
        info->firmwares = array_new(err, gl.gl_pathc);
        if (info->firmwares == NULL) {
                log_err("error creating firmware paths array");
                goto fail;
        }

        for (size_t i = 0; i < gl.gl_pathc; ++i) {
                strcpy(glob_path, firmware_path);
                if (path_append(err, glob_path, basename(gl.gl_pathv[i])) < 0) {
                        log_err("error appending firmware filename to unresolved firmware path");
                        goto fail;
                }
                log_infof("listing firmware path %s", glob_path);
                if ((info->firmwares[i] = xstrdup(err, glob_path)) == NULL) {
                        log_err("error copying firmware path into array");
                        goto fail;
                }
        }

        array_pack(info->firmwares, &info->nfirmwares);

success:
        rv = 0;
fail:
        free(firmware_path);
        globfree(&gl);
        return (rv);
}

static int
lookup_devices(struct error *err, struct dxcore_context *dxcore, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        struct nvc_device_node uvm, uvm_tools, modeset, nvidiactl, dxg, *node;
        int has_dxg = 0;
        int has_nvidiactl = 0;
        int has_uvm = 0;
        int has_uvm_tools = 0;
        int has_modeset = 0;

        if (dxcore->initialized) {
                struct stat dxgDeviceStat;

                if (xstat(err, (char *)MSFT_DXG_DEVICE_PATH, &dxgDeviceStat) < 0) {
                        log_errf("failed to query device information for %s", MSFT_DXG_DEVICE_PATH);
                        return (-1);
                }

                dxg.path = (char *)MSFT_DXG_DEVICE_PATH;
                dxg.id = dxgDeviceStat.st_rdev;
                has_dxg = 1;
        }
        else {
                if (!(flags & OPT_NO_UVM)) {
                        if ((has_uvm = find_device_node(err, root, NV_UVM_DEVICE_PATH, &uvm)) < 0)
                                return (-1);
                        if ((has_uvm_tools = find_device_node(err, root, NV_UVM_TOOLS_DEVICE_PATH, &uvm_tools)) < 0)
                                return (-1);
                }
                if (!(flags & OPT_NO_MODESET)) {
                        modeset.path = (char *)NV_MODESET_DEVICE_PATH;
                        modeset.id = makedev(NV_DEVICE_MAJOR, NV_MODESET_DEVICE_MINOR);
                        has_modeset = 1;
                }
                nvidiactl.path = (char *)NV_CTL_DEVICE_PATH;
                nvidiactl.id = makedev(NV_DEVICE_MAJOR, NV_CTL_DEVICE_MINOR);
                has_nvidiactl = 1;
        }

        info->ndevs = (size_t)(has_dxg + has_nvidiactl + has_uvm + has_uvm_tools + has_modeset);
        info->devs = node = xcalloc(err, info->ndevs, sizeof(*info->devs));
        if (info->devs == NULL)
                return (-1);

        if (has_dxg)
                *(node++) = dxg;
        if (has_nvidiactl)
                *(node++) = nvidiactl;
        if (has_uvm)
                *(node++) = uvm;
        if (has_uvm_tools)
                *(node++) = uvm_tools;
        if (has_modeset)
                *(node++) = modeset;

        for (size_t i = 0; i < info->ndevs; ++i)
                log_infof("listing device %s", info->devs[i].path);
        return (0);
}

static int
lookup_ipcs(struct error *err, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        char **ptr;
        const char *mps;

        info->nipcs = 3;
        info->ipcs = ptr = array_new(err, info->nipcs);
        if (info->ipcs == NULL)
                return (-1);

        if (!(flags & OPT_NO_PERSISTENCED)) {
                if (find_path(err, "ipc", root, NV_PERSISTENCED_SOCKET, ptr++) < 0)
                        return (-1);
        }
        if (!(flags & OPT_NO_FABRICMANAGER)) {
                if (find_path(err, "ipc", root, NV_FABRICMANAGER_SOCKET, ptr++) < 0)
                        return (-1);
        }
        if (!(flags & OPT_NO_MPS)) {
                if ((mps = secure_getenv("CUDA_MPS_PIPE_DIRECTORY")) == NULL)
                        mps = NV_MPS_PIPE_DIR;
                if (find_path(err, "ipc", root, mps, ptr++) < 0)
                        return (-1);
        }
        array_pack(info->ipcs, &info->nipcs);
        return (0);
}

static int
fill_mig_device_info(struct nvc_context *ctx, bool mig_enabled, struct driver_device *drv_device, struct nvc_device *device)
{
        // Initialize local variables.
        struct nvc_mig_device_info *info = &device->mig_devices;
        struct driver_device *mig_device;
        unsigned int count = 0;

        // Clear out the 'gpu_instance_info' struct embedded in the device.
        memset(info, 0, sizeof(*info));

        // If MIG is not enabled, we have nothing more to do, so exit.
        if (!mig_enabled)
            return 0;

        // Otherwise, get the max count of MIG devices for the given device
        // from the driver.
        if (driver_get_device_max_mig_device_count(&ctx->err, drv_device, &count) < 0)
                goto fail;

        // Allocate space in 'devices' to hold all of the MIG devices.
        if ((info->devices = xcalloc(&ctx->err, count, sizeof(struct nvc_mig_device))) == NULL)
                goto fail;

        // Populate 'mig_device_info' with information about the MIG devices
        // pulled from the driver.
        for (unsigned int i = 0; i < count; ++i) {
                // Get a reference to the MIG device at this index.
                if (driver_get_device_mig_device(&ctx->err, drv_device, i, &mig_device) < 0)
                        goto fail;

               // If no MIG device exists at this index, then we are done. Due
               // to races, there may (temporarily) be more devices further on
               // in the list, but we won't detect them.
               if (mig_device == NULL)
                        break;

                // Get the ID of the GPU Instance for this MIG device from the driver.
                if (driver_get_device_gpu_instance_id(&ctx->err, mig_device, &info->devices[i].gi) < 0)
                        goto fail;

                // Get the ID of the Compute Instance for this MIG device from the driver.
                if (driver_get_device_compute_instance_id(&ctx->err, mig_device, &info->devices[i].ci) < 0)
                        goto fail;

                // Set a reference back to the device associated with the
                // current MIG device.
                info->devices[i].parent = device;

                // Populate the UUID of the MIG device.
                if (driver_get_device_uuid(&ctx->err, mig_device, &info->devices[i].uuid) < 0)
                        goto fail;

                // Build a path to the MIG caps inside '/proc' associated
                // with GPU Instance of the MIG device and set it inside
                // 'info->devices[i]'.
                if (xasprintf(&ctx->err, &info->devices[i].gi_caps_path, NV_GPU_INST_CAPS_PATH, minor(device->node.id), info->devices[i].gi) < 0)
                        goto fail;

                // Build a path to the MIG caps inside '/proc' associated
                // with the Compute Instance of the MIG device and set it
                // inside 'info->devices[i]'.
                if (xasprintf(&ctx->err, &info->devices[i].ci_caps_path, NV_COMP_INST_CAPS_PATH, minor(device->node.id), info->devices[i].gi, info->devices[i].ci) < 0)
                        goto fail;

                // If we made it to here, update the total count of MIG dervices by 1
                info->ndevices++;
        }

        return (0);

 fail:
        // On failure, free all driver memory associated with the GPU Instances
        // and also free any memory allocated for the 'gpu_instance_info'
        // itself before returning.
        clear_mig_device_info(info);
        return (-1);
}

static void
clear_mig_device_info(struct nvc_mig_device_info *info)
{
        // Walk through each element in the MIG devices array and free any
        // memory associated with it.
        for (size_t i = 0; info->devices != NULL && i < info->ndevices; ++i) {
                // Free the memory allocated for the UUID of the mig device.
                free(info->devices[i].uuid);
                // Free the memory allocated for the GPU Instance caps path.
                free(info->devices[i].gi_caps_path);
                // Free the memory allocated for the Compute Instance caps path.
                free(info->devices[i].ci_caps_path);
        }

        // Free the memory for the device array itself.
        free(info->devices);

        // Zero out the info struct.
        memset(info, 0, sizeof(*info));
}

static int
init_nvc_device(struct nvc_context *ctx, unsigned int index, struct nvc_device *gpu)
{
        struct driver_device *dev;
        struct error *err = &ctx->err;
        bool mig_enabled;
        unsigned int minor;

        if (driver_get_device(err, index, &dev) < 0)
                goto fail;
        if (driver_get_device_model(err, dev, &gpu->model) < 0)
                goto fail;
        if (driver_get_device_uuid(err, dev, &gpu->uuid) < 0)
                goto fail;
        if (driver_get_device_busid(err, dev, &gpu->busid) < 0)
                goto fail;
        if (driver_get_device_arch(err, dev, &gpu->arch) < 0)
                goto fail;
        if (driver_get_device_brand(err, dev, &gpu->brand) < 0)
                goto fail;
        if (ctx->dxcore.initialized)
        {
                // No Device associated to a WSL GPU. Everything uses /dev/dxg
                gpu->node.path = NULL;
                minor = 0;

                // No MIG support for WSL
                gpu->mig_capable = 0;
                gpu->mig_caps_path = NULL;
                gpu->mig_devices.ndevices = 0;
                gpu->mig_devices.devices = NULL;

                log_infof("listing dxcore adapter %d (%s at %s)", index, gpu->uuid, gpu->busid);
        }
        else
        {
                if (driver_get_device_minor(err, dev, &minor) < 0)
                        goto fail;
                if (xasprintf(err, &gpu->mig_caps_path, NV_GPU_CAPS_PATH, minor) < 0)
                        goto fail;
                if (xasprintf(err, &gpu->node.path, NV_DEVICE_PATH, minor) < 0)
                        goto fail;
                if (driver_get_device_mig_capable(err, dev, &gpu->mig_capable) < 0)
                        goto fail;
                if (driver_get_device_mig_enabled(err, dev, &mig_enabled) < 0)
                        goto fail;
                gpu->node.id = makedev(NV_DEVICE_MAJOR, minor);

                if (fill_mig_device_info(ctx, mig_enabled, dev, gpu) < 0)
                    goto fail;

                log_infof("listing device %s (%s at %s)", gpu->node.path, gpu->uuid, gpu->busid);
        }

        return 0;

 fail:
        return (-1);
}


bool
match_binary_flags(const char *bin, int32_t flags)
{
        if ((flags & OPT_UTILITY_BINS) && str_array_match_prefix(bin, utility_bins, nitems(utility_bins)))
                return (true);
        if ((flags & OPT_COMPUTE_BINS) && str_array_match_prefix(bin, compute_bins, nitems(compute_bins)))
                return (true);
        return (false);
}

bool
match_library_flags(const char *lib, int32_t flags)
{
        if (str_array_match_prefix(lib, dxcore_libs, nitems(dxcore_libs)))
                return (true);
        if ((flags & OPT_UTILITY_LIBS) && str_array_match_prefix(lib, utility_libs, nitems(utility_libs)))
                return (true);
        if ((flags & OPT_COMPUTE_LIBS) && str_array_match_prefix(lib, compute_libs, nitems(compute_libs)))
                return (true);
        if ((flags & OPT_VIDEO_LIBS) && str_array_match_prefix(lib, video_libs, nitems(video_libs)))
                return (true);
        if ((flags & OPT_GRAPHICS_LIBS) && (str_array_match_prefix(lib, graphics_libs, nitems(graphics_libs)) ||
            str_array_match_prefix(lib, graphics_libs_glvnd, nitems(graphics_libs_glvnd)) ||
            str_array_match_prefix(lib, graphics_libs_compat, nitems(graphics_libs_compat))))
                return (true);
        if ((flags & OPT_NGX_LIBS) && str_array_match_prefix(lib, ngx_libs, nitems(ngx_libs)))
                return (true);
        return (false);
}

struct nvc_driver_info *
nvc_driver_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_driver_info *info;
        int32_t flags;

        if (validate_context(ctx) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_driver_opts;
        if ((flags = options_parse(&ctx->err, opts, driver_opts, nitems(driver_opts))) < 0)
                return (NULL);

        log_infof("requesting driver information with '%s'", opts);
        if ((info = xcalloc(&ctx->err, 1, sizeof(*info))) == NULL)
                return (NULL);

        if (driver_get_rm_version(&ctx->err, &info->nvrm_version) < 0)
                goto fail;
        if (driver_get_cuda_version(&ctx->err, &info->cuda_version) < 0)
                goto fail;
        if (lookup_paths(&ctx->err, &ctx->dxcore, info, ctx->cfg.root, flags, ctx->cfg.ldcache) < 0)
                goto fail;
        if (lookup_devices(&ctx->err, &ctx->dxcore, info, ctx->cfg.root, flags) < 0)
                goto fail;
        if (lookup_ipcs(&ctx->err, info, ctx->cfg.root, flags) < 0)
                goto fail;
        return (info);

 fail:
        nvc_driver_info_free(info);
        return (NULL);
}

void
nvc_driver_info_free(struct nvc_driver_info *info)
{
        if (info == NULL)
                return;
        free(info->nvrm_version);
        free(info->cuda_version);
        array_free(info->bins, info->nbins);
        array_free(info->libs, info->nlibs);
        array_free(info->libs32, info->nlibs32);
        array_free(info->ipcs, info->nipcs);
        array_free(info->firmwares, info->nfirmwares);
        free(info->devs);
        free(info);
}

struct nvc_device_info *
nvc_device_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_device_info *info;
        struct nvc_device *gpu;
        unsigned int n;
        int rv = -1;

        /*int32_t flags;*/

        if (validate_context(ctx) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_device_opts;
        /*
        if ((flags = options_parse(&ctx->err, opts, device_opts, nitems(device_opts))) < 0)
                return (NULL);
        */

        log_infof("requesting device information with '%s'", opts);
        if ((info = xcalloc(&ctx->err, 1, sizeof(*info))) == NULL)
                return (NULL);

        if (driver_get_device_count(&ctx->err, &n) < 0)
            goto fail;

        info->ngpus = n;
        info->gpus = gpu = xcalloc(&ctx->err, info->ngpus, sizeof(*info->gpus));
        if (info->gpus == NULL)
                goto fail;

        for (unsigned int i = 0; i < n; ++i, ++gpu) {
                rv = init_nvc_device(ctx, i, gpu);
                if (rv < 0) goto fail;
        }

        return (info);

 fail:
        nvc_device_info_free(info);
        return (NULL);
}

void
nvc_device_info_free(struct nvc_device_info *info)
{
        if (info == NULL)
                return;
        for (size_t i = 0; info->gpus != NULL && i < info->ngpus; ++i) {
                free(info->gpus[i].model);
                free(info->gpus[i].uuid);
                free(info->gpus[i].busid);
                free(info->gpus[i].arch);
                free(info->gpus[i].brand);
                free(info->gpus[i].mig_caps_path);
                free(info->gpus[i].node.path);
                clear_mig_device_info(&info->gpus[i].mig_devices);
        }
        free(info->gpus);
        free(info);
}

int
nvc_nvcaps_style(void)
{
        if (nvidia_get_chardev_major(NV_CAPS_MODULE_NAME) >= 0)
                return NVC_NVCAPS_STYLE_DEV;
        if (file_exists(NULL, NV_PROC_DRIVER_CAPS) >= 0)
                return NVC_NVCAPS_STYLE_PROC;
        return NVC_NVCAPS_STYLE_NONE;
}

int
nvc_nvcaps_device_from_proc_path(struct nvc_context *ctx, const char *cap_path, struct nvc_device_node *node)
{
        char abs_cap_path[PATH_MAX];
        char dev_name[PATH_MAX];
        int major, minor;
        int rv = -1;

        if (path_join(&ctx->err, abs_cap_path, ctx->cfg.root, cap_path) < 0)
                goto fail;

        if (nvidia_cap_get_device_file_attrs(abs_cap_path, &major, &minor, dev_name) == 0) {
                error_set(&ctx->err, "unable to get cap device attributes: %s", cap_path);
                goto fail;
        }

        if ((node->path = xstrdup(&ctx->err, dev_name)) == NULL)
                goto fail;
        node->id = makedev((unsigned int)major, (unsigned int)minor);

        rv = 0;

fail:
        return (rv);
}
