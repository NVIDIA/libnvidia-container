/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <gnu/lib-names.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pci-enum.h>
#include <nvidia-modprobe-utils.h>

#include "nvc_internal.h"

#include "common.h"
#include "driver.h"
#include "dxcore.h"
#include "debug.h"
#include "error.h"
#ifdef WITH_NVCGO
#include "nvcgo.h"
#endif
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

static int init_within_userns(struct error *);
static int load_kernel_modules(struct error *, const char *, const struct nvc_imex_info *, int32_t);
static int copy_config(struct error *, struct nvc_context *, const struct nvc_config *);

const char interpreter[] __attribute__((section(".interp"))) = LIB_DIR "/" LD_SO;

const struct __attribute__((__packed__)) {
        Elf64_Nhdr hdr;
        uint32_t desc[5];
} abitag __attribute__((section (".note.ABI-tag"))) = {
        {0x04, 0x10, 0x01},
        {0x554e47, 0x0, 0x3, 0xa, 0x0}, /* GNU Linux 3.10.0 */
};

static const struct nvc_version version = {
        NVC_MAJOR,
        NVC_MINOR,
        NVC_PATCH,
        NVC_VERSION,
};

void
nvc_entrypoint(void)
{
        printf("version: %s\n", NVC_VERSION);
        printf("build date: %s\n", BUILD_DATE);
        printf("build revision: %s\n", BUILD_REVISION);
        printf("build compiler: %s\n", BUILD_COMPILER);
        printf("build platform: %s\n", BUILD_PLATFORM);
        printf("build flags: %s\n", BUILD_FLAGS);
        exit(EXIT_SUCCESS);
}

const struct nvc_version *
nvc_version(void)
{
        return (&version);
}

struct nvc_config *
nvc_config_new(void)
{
        struct nvc_config *cfg;

        if ((cfg = calloc(1, sizeof(*cfg))) == NULL)
                return (NULL);
        cfg->uid = (uid_t)-1;
        cfg->gid = (gid_t)-1;
        return (cfg);
}

void
nvc_config_free(struct nvc_config *cfg)
{
        if (cfg == NULL)
                return;
        free(cfg);
}

struct nvc_context *
nvc_context_new(void)
{
        struct nvc_context *ctx;

        if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
                return (NULL);
        return (ctx);
}

void
nvc_context_free(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return;
        error_reset(&ctx->err);
        free(ctx);
}

static int
init_within_userns(struct error *err)
{
        char buf[64];
        uint32_t start, pstart, len;

        if (file_read_line(err, PROC_UID_MAP_PATH(PROC_SELF), buf, sizeof(buf)) < 0)
                return ((err->code == ENOENT) ? false : -1); /* User namespace unsupported. */
        if (str_empty(buf))
                return (true); /* User namespace uninitialized. */
        if (sscanf(buf, "%"PRIu32" %"PRIu32" %"PRIu32, &start, &pstart, &len) < 3) {
                error_setx(err, "invalid map file: %s", PROC_UID_MAP_PATH(PROC_SELF));
                return (-1);
        }
        if (start != 0 || pstart != 0 || len != UINT32_MAX)
                return (true); /* User namespace mapping exists. */

        if (file_read_line(err, PROC_GID_MAP_PATH(PROC_SELF), buf, sizeof(buf)) < 0)
                return ((err->code == ENOENT) ? false : -1);
        if (str_empty(buf))
                return (true);
        if (sscanf(buf, "%"PRIu32" %"PRIu32" %"PRIu32, &start, &pstart, &len) < 3) {
                error_setx(err, "invalid map file: %s", PROC_GID_MAP_PATH(PROC_SELF));
                return (-1);
        }
        if (start != 0 || pstart != 0 || len != UINT32_MAX)
                return (true);

        if (file_read_line(err, PROC_SETGROUPS_PATH(PROC_SELF), buf, sizeof(buf)) < 0)
                return ((err->code == ENOENT) ? false : -1);
        if (str_has_prefix(buf, "deny"))
                return (true);

        return (false);
}

static int
mig_nvcaps_mknodes(struct error *err, int num_gpus) {
        FILE *fp;
        char line[PATH_MAX];
        char path[PATH_MAX];
        int gpu = -1, gi = -1, ci = -1, mig_minor = -1;
        int rv = -1;

        // If the NV_CAPS_MIG_MINORS_PATH does not exist, then we are not on a
        // MIG capable machine, so there is nothing to do.
        if (!file_exists(NULL, NV_CAPS_MIG_MINORS_PATH))
                return (0);

        // Open NV_CAPS_MIG_MINORS_PATH for walking.
        // The format of this file is discussed in:
        //     https://docs.nvidia.com/datacenter/tesla/mig-user-guide/index.html#unique_1576522674
        if ((fp = fopen(NV_CAPS_MIG_MINORS_PATH, "r")) == NULL) {
                error_setx(err, "unable to open: %s", NV_CAPS_MIG_MINORS_PATH);
                return (-1);
        }

        // Walk through each line of NV_CAPS_MIG_MINORS_PATH
        memset(line, 0, PATH_MAX);
        memset(path, 0, PATH_MAX);
        while (fgets(line, PATH_MAX - 1, fp)) {
                // Look for a ci access entry and construct a path into /proc from it
                if (sscanf(line, "gpu%d/gi%d/ci%d/access %d", &gpu, &gi, &ci, &mig_minor) == 4) {
                        if (gpu >= num_gpus)
                                continue;
                        if (sprintf(path, NV_COMP_INST_CAPS_PATH "/" NV_MIG_ACCESS_FILE, gpu, gi, ci) < 0) {
                                error_setx(err, "error constructing path for ci access file");
                                goto fail;
                        }
                // Look for a gi access entry and construct a path into /proc from it
                } else if (sscanf(line, "gpu%d/gi%d/access %d", &gpu, &gi, &mig_minor) == 3) {
                        if (gpu >= num_gpus)
                                continue;
                        if (sprintf(path, NV_GPU_INST_CAPS_PATH "/" NV_MIG_ACCESS_FILE, gpu, gi) < 0) {
                                error_setx(err, "error constructing path for gi access file");
                                goto fail;
                        }
                // Look for a mig config entry and construct a path into /proc from it
                } else if (sscanf(line, "config %d", &mig_minor) == 1) {
                        if (sprintf(path, NV_MIG_CAPS_PATH "/" NV_MIG_CONFIG_FILE) < 0) {
                                error_setx(err, "error constructing path for mig config file");
                                goto fail;
                        }
                // Look for a mig monitor entry and construct a path into /proc from it
                } else if (sscanf(line, "monitor %d", &mig_minor) == 1) {
                        if (sprintf(path, NV_MIG_CAPS_PATH "/" NV_MIG_MONITOR_FILE) < 0) {
                                error_setx(err, "error constructing path for mig monitor file");
                                goto fail;
                        }
                // We encountered an unexpected pattern, so error out
                } else {
                        error_setx(err, "unexpected line in mig-monitors file: %s", line);
                        goto fail;
                }

                // This file contains entries for all possible MIG nvcaps on up
                // to 32 GPUs. If the newly constructed path does not exist,
                // then just move on because there are many entries in this
                // file that will not be present on the machine.
                if (!file_exists(NULL, path))
                        continue;

                // Call into nvidia-modprobe code to perform the mknod() on
                // /dev/nvidia-caps/nvidia-cap<mig_minor> from the canonical
                // /proc path we constructed.
                log_infof("running mknod for " NV_CAPS_DEVICE_PATH " from %s", mig_minor, path);
                if (nvidia_cap_mknod(path, &mig_minor) == 0) {
                        error_setx(err, "error running mknod for nvcap: %s", path);
                        goto fail;
                }
        }
        rv = 0;

fail:
        fclose(fp);
        return (rv);
}

static int
load_kernel_modules(struct error *err, const char *root, const struct nvc_imex_info *imex, int32_t flags)
{
        int userns;
        pid_t pid;
        struct pci_id_match devs = {
                0x10de,        /* vendor (NVIDIA) */
                PCI_MATCH_ANY, /* device */
                PCI_MATCH_ANY, /* subvendor */
                PCI_MATCH_ANY, /* subdevice */
                0x0300,        /* class (display) */
                0xff00,        /* class mask (any subclass) */
                0,             /* match count */
        };

        /*
         * Prevent loading the kernel modules if we are inside a user namespace because we could potentially adjust the host
         * device nodes based on the (wrong) driver registry parameters and we won't have the right capabilities anyway.
         */
        if ((userns = init_within_userns(err)) < 0)
                return (-1);
        if (userns) {
                log_warn("skipping kernel modules load due to user namespace");
                return (0);
        }

        if (pci_enum_match_id(&devs) != 0 || devs.num_matches == 0)
                log_warn("failed to detect NVIDIA devices");

        if ((pid = fork()) < 0) {
                error_set(err, "process creation failed");
                return (-1);
        }
        if (pid == 0) {
                if (!str_equal(root, "/")) {
                        if (chroot(root) < 0 || chdir("/") < 0) {
                                log_errf("failed to change root directory: %s", strerror(errno));
                                log_warn("skipping kernel modules load due to failure");
                                _exit(EXIT_FAILURE);
                        }
                }
                if (perm_set_capabilities(NULL, CAP_INHERITABLE, &(cap_value_t){CAP_SYS_MODULE}, 1) < 0) {
                        log_warn("failed to set inheritable capabilities");
                        log_warn("skipping kernel modules load due to failure");
                        _exit(EXIT_FAILURE);
                }

                log_info("loading kernel module nvidia");
                if (nvidia_modprobe(0) == 0)
                        log_err("could not load kernel module nvidia");
                else {
                        log_info("running mknod for " NV_CTL_DEVICE_PATH);
                        if (nvidia_mknod(NV_CTL_DEVICE_MINOR) == 0)
                                log_err("could not create kernel module device node");
                        for (int i = 0; i < (int)devs.num_matches; ++i) {
                                log_infof("running mknod for " NV_DEVICE_PATH, i);
                                if (nvidia_mknod(i) == 0)
                                        log_err("could not create kernel module device node");
                        }
                        log_info("running mknod for all nvcaps in " NV_CAPS_DEVICE_DIR);
                        if (mig_nvcaps_mknodes(err, devs.num_matches) < 0)
                                log_errf("could not create kernel module device nodes: %s", err->msg);

                        if (!(flags & OPT_NO_CREATE_IMEX_CHANNELS)) {
                                for (int i = 0; i < (int)imex->nchans; ++i) {
                                        log_infof("running mknod for " NV_CAPS_IMEX_DEVICE_PATH, imex->chans[i].id);
                                        if (nvidia_cap_imex_channel_mknod(imex->chans[i].id) == 0)
                                                log_errf("could not mknod for IMEX channel %d", imex->chans[i].id);
                                }
                        }
                        error_reset(err);
                }

                log_info("loading kernel module nvidia_uvm");
                if (nvidia_uvm_modprobe() == 0)
                        log_err("could not load kernel module nvidia_uvm");
                else {
                        log_info("running mknod for " NV_UVM_DEVICE_PATH);
                        if (nvidia_uvm_mknod(0) == 0)
                                log_err("could not create kernel module device node");
                }

                log_info("loading kernel module nvidia_modeset");
                if (nvidia_modeset_modprobe() == 0)
                        log_err("could not load kernel module nvidia_modeset");
                else {
                        log_info("running mknod for " NV_MODESET_DEVICE_PATH);
                        if (nvidia_modeset_mknod() == 0)
                                log_err("could not create kernel module device node");
                }

                _exit(EXIT_SUCCESS);
        }
        waitpid(pid, NULL, 0);

        return (0);
}

static int
copy_config(struct error *err, struct nvc_context *ctx, const struct nvc_config *cfg)
{
        const char *root, *ldcache;
        uint32_t uid, gid;

        root = (cfg->root != NULL) ? cfg->root : "/";
        if ((ctx->cfg.root = xstrdup(err, root)) == NULL)
                return (-1);

        ldcache = (cfg->ldcache != NULL) ? cfg->ldcache : LDCACHE_PATH;
        if ((ctx->cfg.ldcache = xstrdup(err, ldcache)) == NULL)
                return (-1);

        if (cfg->uid != (uid_t)-1)
                ctx->cfg.uid = cfg->uid;
        else {
                if (file_read_uint32(err, PROC_OVERFLOW_UID, &uid) < 0)
                        return (-1);
                ctx->cfg.uid = (uid_t)uid;
        }
        if (cfg->gid != (gid_t)-1)
                ctx->cfg.gid = cfg->gid;
        else {
                if (file_read_uint32(err, PROC_OVERFLOW_GID, &gid) < 0)
                        return (-1);
                ctx->cfg.gid = (gid_t)gid;
        }

        if (cfg->imex.nchans > 0) {
                if ((ctx->cfg.imex.chans = xcalloc(err, cfg->imex.nchans, sizeof(*ctx->cfg.imex.chans))) == NULL)
                        return (-1);
        }
        for (size_t i = 0; i < cfg->imex.nchans; ++i) {
                ctx->cfg.imex.chans[i] = cfg->imex.chans[i];
        }
        ctx->cfg.imex.nchans = cfg->imex.nchans;

        log_infof("using root %s", ctx->cfg.root);
        log_infof("using ldcache %s", ctx->cfg.ldcache);
        log_infof("using unprivileged user %"PRIu32":%"PRIu32, (uint32_t)ctx->cfg.uid, (uint32_t)ctx->cfg.gid);
        for (size_t i = 0; i < ctx->cfg.imex.nchans; ++i) {
            log_infof("using IMEX channel %d", ctx->cfg.imex.chans[i].id);
        }
        return (0);
}

int
nvc_init(struct nvc_context *ctx, const struct nvc_config *cfg, const char *opts)
{
        int32_t flags;
        char path[PATH_MAX];

        if (ctx == NULL)
                return (-1);
        if (ctx->initialized)
                return (0);
        if (cfg == NULL)
                cfg = &(struct nvc_config){NULL, NULL, (uid_t)-1, (gid_t)-1, {0}};
        if (validate_args(ctx, !str_empty(cfg->ldcache) && !str_empty(cfg->root)) < 0)
                return (-1);
        if (opts == NULL)
                opts = default_library_opts;
        if ((flags = options_parse(&ctx->err, opts, library_opts, nitems(library_opts))) < 0)
                return (-1);

        log_open(secure_getenv("NVC_DEBUG_FILE"));
        log_infof("initializing library context (version=%s, build=%s)", NVC_VERSION, BUILD_REVISION);

        memset(&ctx->cfg, 0, sizeof(ctx->cfg));
        ctx->mnt_ns = -1;

        if (copy_config(&ctx->err, ctx, cfg) < 0)
                goto fail;
        if (xsnprintf(&ctx->err, path, sizeof(path), PROC_NS_PATH(PROC_SELF), "mnt") < 0)
                goto fail;
        if ((ctx->mnt_ns = xopen(&ctx->err, path, O_RDONLY|O_CLOEXEC)) < 0)
                goto fail;

        // Initialize dxcore first to check if we are on a platform that supports dxcore.
        // If we are on not a platform with dxcore we will load the nvidia kernel modules and
        // use the nvidia libraries directly. If we do have access to dxcore, we will
        // do all the initial setup using dxcore and piggy back on the dxcore infrastructure
        // to enumerate gpus and find the driver location
        log_info("attempting to load dxcore to see if we are running under Windows Subsystem for Linux (WSL)");
        if (dxcore_init_context(&ctx->dxcore) < 0) {
                log_info("dxcore initialization failed, continuing assuming a non-WSL environment");
                ctx->dxcore.initialized = 0;
        } else if (ctx->dxcore.adapterCount == 0) {
                log_err("dxcore initialization succeeded but no adapters were found");
                error_setx(&ctx->err, "WSL environment detected but no adapters were found");
                goto fail;
        }

        if (flags & OPT_LOAD_KMODS) {
                if (ctx->dxcore.initialized)
                        log_warn("skipping kernel modules load on WSL");
                else if (load_kernel_modules(&ctx->err, ctx->cfg.root, &ctx->cfg.imex, flags) < 0)
                        goto fail;
        }

        if (driver_init(&ctx->err, &ctx->dxcore, ctx->cfg.root, ctx->cfg.uid, ctx->cfg.gid) < 0)
                goto fail;

        #ifdef WITH_NVCGO
        if (nvcgo_init(&ctx->err) < 0)
                goto fail;
        #endif

        ctx->initialized = true;
        return (0);

 fail:
        free(ctx->cfg.root);
        free(ctx->cfg.ldcache);
        free(ctx->cfg.imex.chans);
        xclose(ctx->mnt_ns);
        return (-1);
}

int
nvc_shutdown(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (-1);

        log_info("shutting down library context");

        int rv = 0;
        #ifdef WITH_NVCGO
        if (nvcgo_shutdown(&ctx->err) < 0) {
                log_warnf("error shutting down nvcgo rpc service: %s", ctx->err.msg);
                rv = -1;
        }
        #endif
        if (driver_shutdown(&ctx->err) < 0) {
                log_warnf("error shutting down driver rpc service: %s", ctx->err.msg);
                rv = -1;
        }

        if (!ctx->initialized)
                return (rv);

        if (ctx->dxcore.initialized)
                dxcore_deinit_context(&ctx->dxcore);

        free(ctx->cfg.root);
        free(ctx->cfg.ldcache);
        free(ctx->cfg.imex.chans);
        xclose(ctx->mnt_ns);

        memset(&ctx->cfg, 0, sizeof(ctx->cfg));
        ctx->mnt_ns = -1;

        log_close();
        ctx->initialized = false;
        return (rv);
}

const char *
nvc_error(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (NULL);
        if (ctx->err.code != 0 && ctx->err.msg == NULL)
                return ("unknown error");
        return (ctx->err.msg);
}
