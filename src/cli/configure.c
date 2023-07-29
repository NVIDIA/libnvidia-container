/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <err.h>
#include <stdlib.h>

#include "cli.h"
#include "dsl.h"

static error_t configure_parser(int, char *, struct argp_state *);
static int check_cuda_version(const struct dsl_data *, enum dsl_comparator, const char *);
static int check_driver_version(const struct dsl_data *, enum dsl_comparator, const char *);
static int check_device_arch(const struct dsl_data *, enum dsl_comparator, const char *);
static int check_device_brand(const struct dsl_data *, enum dsl_comparator, const char *);

const struct argp configure_usage = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"pid", 'p', "PID", 0, "Container PID", -1},
                {"device", 'd', "ID", 0, "Device UUID(s) or index(es) to isolate", -1},
                {"require", 'r', "EXPR", 0, "Check container requirements", -1},
                {"ldconfig", 'l', "PATH", 0, "Path to the ldconfig binary", -1},
                {"compute", 'c', NULL, 0, "Enable compute capability", -1},
                {"utility", 'u', NULL, 0, "Enable utility capability", -1},
                {"video", 'v', NULL, 0, "Enable video capability", -1},
                {"graphics", 'g', NULL, 0, "Enable graphics capability", -1},
                {"display", 'D', NULL, 0, "Enable display capability", -1},
                {"ngx", 'n', NULL, 0, "Enable ngx capability", -1},
                {"compat32", 0x80, NULL, 0, "Enable 32bits compatibility", -1},
                {"mig-config", 0x81, "ID", 0, "Enable configuration of MIG devices", -1},
                {"mig-monitor", 0x82, "ID", 0, "Enable monitoring of MIG devices", -1},
                {"no-cgroups", 0x83, NULL, 0, "Don't use cgroup enforcement", -1},
                {"no-devbind", 0x84, NULL, 0, "Don't bind mount devices", -1},
                {0},
        },
        configure_parser,
        "ROOTFS",
        "Configure a container with GPU support by exposing device drivers to it.\n\n"
        "This command enters the namespace of the container process referred by PID (or the current parent process if none specified) "
        "and performs the necessary steps to ensure that the given capabilities are available inside the container.\n"
        "It is assumed that the container has been created but not yet started, and the host filesystem is accessible (i.e. chroot/pivot_root hasn't been called).",
        NULL,
        NULL,
        NULL,
};

static const struct dsl_rule rules[] = {
        {"cuda", &check_cuda_version},
        {"driver", &check_driver_version},
        {"arch", &check_device_arch},
        {"brand", &check_device_brand},
};

static error_t
configure_parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'p':
                if (str_to_pid(&err, arg, &ctx->pid) < 0)
                        goto fatal;
                break;
        case 'd':
                if (str_join(&err, &ctx->devices, arg, ",") < 0)
                        goto fatal;
                break;
        case 'r':
                // Check for some special-purpose "requires" flags on tegra devices.
                if (libnvc.version()->major == 0) {
                        // If there is a special-purpose "requires" for
                        // 'csv-mounts=all', then add the 'jetpack-mount-all'
                        // container flag to tell the library to mount all CSV
                        // files included in the jetpack release, instead of
                        // just the base files.
                        if (strncmp(arg, "csv-mounts=all", 14) == 0) {
                                if (str_join(&err, &ctx->container_flags, "jetpack-mount-all", " ") < 0)
                                        goto fatal;
                                break;
                        }
                        // The 'base-only' setting is now the default for
                        // tegra devices, but we need to keep this case here to
                        // make sure we skip over it if encountered while
                        // parsing the other "requires" arguments.
                        if (strncmp(arg, "base-only", 9) == 0) {
                                if (str_join(&err, &ctx->container_flags, "jetpack-base-only", " ") < 0)
                                        goto fatal;
                                break;
                        }
                }
                if (ctx->nreqs >= nitems(ctx->reqs)) {
                        error_setx(&err, "too many requirements");
                        goto fatal;
                }
                ctx->reqs[ctx->nreqs++] = arg;
                break;
        case 'l':
                ctx->ldconfig = arg;
                break;
        case 'c':
                if (str_join(&err, &ctx->container_flags, "compute", " ") < 0)
                        goto fatal;
                break;
        case 'u':
                if (str_join(&err, &ctx->container_flags, "utility", " ") < 0)
                        goto fatal;
                break;
        case 'v':
                if (str_join(&err, &ctx->container_flags, "video", " ") < 0)
                        goto fatal;
                break;
        case 'g':
                if (str_join(&err, &ctx->container_flags, "graphics", " ") < 0)
                        goto fatal;
                break;
        case 'D':
                if (str_join(&err, &ctx->container_flags, "display", " ") < 0)
                        goto fatal;
                break;
        case 'n':
                if (libnvc.version()->major == 0)
                        break;
                if (str_join(&err, &ctx->container_flags, "ngx", " ") < 0)
                        goto fatal;
                break;
        case 0x80:
                if (str_join(&err, &ctx->container_flags, "compat32", " ") < 0)
                        goto fatal;
                break;
        case 0x81:
                if (str_join(&err, &ctx->mig_config, arg, ",") < 0)
                        goto fatal;
                break;
        case 0x82:
                if (str_join(&err, &ctx->mig_monitor, arg, ",") < 0)
                        goto fatal;
                break;
        case 0x83:
                if (str_join(&err, &ctx->container_flags, "no-cgroups", " ") < 0)
                        goto fatal;
                break;
        case 0x84:
                if (str_join(&err, &ctx->container_flags, "no-devbind", " ") < 0)
                        goto fatal;
                break;
        case ARGP_KEY_ARG:
                if (state->arg_num > 0)
                        argp_usage(state);
                if (arg[0] != '/' || str_equal(arg, "/")) {
                        error_setx(&err, "invalid rootfs directory");
                        goto fatal;
                }
                ctx->rootfs = arg;
                break;
        case ARGP_KEY_SUCCESS:
                if (ctx->pid > 0) {
                        if (str_join(&err, &ctx->container_flags, "supervised", " ") < 0)
                                goto fatal;
                } else {
                        ctx->pid = getppid();
                        if (str_join(&err, &ctx->container_flags, "standalone", " ") < 0)
                                goto fatal;
                }
                break;
        case ARGP_KEY_END:
                if (state->arg_num < 1)
                        argp_usage(state);
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);

 fatal:
        errx(EXIT_FAILURE, "input error: %s", err.msg);
        return (0);
}

static int
check_cuda_version(const struct dsl_data *data, enum dsl_comparator cmp, const char *version)
{
        /* XXX No device is visible, assume the cuda_version is ok. */
        if (data->drv == NULL)
                return (true);
        if (data->drv->cuda_version == NULL)
                return (true);
        return (dsl_compare_version(data->drv->cuda_version, cmp, version));
}

static int
check_driver_version(const struct dsl_data *data, enum dsl_comparator cmp, const char *version)
{
        /* XXX No device is visible, assume the nvrm_version is ok. */
        if (data->drv == NULL)
                return (true);
        if (data->drv->nvrm_version == NULL)
                return (true);
        return (dsl_compare_version(data->drv->nvrm_version, cmp, version));
}

static int
check_device_arch(const struct dsl_data *data, enum dsl_comparator cmp, const char *arch)
{
        /* XXX No device is visible, assume the arch is ok. */
        if (data->dev == NULL)
                return (true);
        if (data->dev->arch== NULL)
                return (true);
        return (dsl_compare_version(data->dev->arch, cmp, arch));
}

static int
check_device_brand(const struct dsl_data *data, enum dsl_comparator cmp, const char *brand)
{
        /* XXX No device is visible, assume the brand is ok. */
        if (data->dev == NULL)
                return (true);
        if (data->dev->brand == NULL)
                return (true);
        return (dsl_compare_string(data->dev->brand, cmp, brand));
}

int
configure_command(const struct context *ctx)
{
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        struct nvc_container *cnt = NULL;
        struct nvc_container_config *cnt_cfg = NULL;
        bool eval_reqs = true;
        struct devices devices = {0};
        struct devices mig_config_devices = {0};
        struct devices mig_monitor_devices = {0};
        struct error err = {0};
        int rv = EXIT_FAILURE;

        if (perm_set_capabilities(&err, CAP_PERMITTED, pcaps, nitems(pcaps)) < 0 ||
            perm_set_capabilities(&err, CAP_INHERITABLE, NULL, 0) < 0 ||
            perm_set_bounds(&err, bcaps, nitems(bcaps)) < 0) {
                warnx("permission error: %s", err.msg);
                return (rv);
        }

        /* Initialize the library and container contexts. */
        int c = ctx->load_kmods ? NVC_INIT_KMODS : NVC_INIT;
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[c], ecaps_size(c)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((nvc = libnvc.context_new()) == NULL ||
            (nvc_cfg = libnvc.config_new()) == NULL ||
            (cnt_cfg = libnvc.container_config_new(ctx->pid, ctx->rootfs)) == NULL) {
                warn("memory allocation failed");
                goto fail;
        }
        nvc->no_pivot = ctx->no_pivot;
        nvc_cfg->uid = ctx->uid;
        nvc_cfg->gid = ctx->gid;
        nvc_cfg->root = ctx->root;
        nvc_cfg->ldcache = ctx->ldcache;
        if (libnvc.init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", libnvc.error(nvc));
                goto fail;
        }
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_CONTAINER], ecaps_size(NVC_CONTAINER)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        cnt_cfg->ldconfig = ctx->ldconfig;
        if ((cnt = libnvc.container_new(nvc, cnt_cfg, ctx->container_flags)) == NULL) {
                warnx("container error: %s", libnvc.error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_INFO], ecaps_size(NVC_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = libnvc.driver_info_new(nvc, NULL)) == NULL ||
            (dev = libnvc.device_info_new(nvc, NULL)) == NULL) {
                warnx("detection error: %s", libnvc.error(nvc));
                goto fail;
        }

        /* Allocate space for selecting GPU devices and MIG devices */
        if (new_devices(&err, dev, &devices) < 0) {
                warn("memory allocation failed: %s", err.msg);
                goto fail;
        }

        /* Allocate space for selecting which devices are available for MIG config */
        if (new_devices(&err, dev, &mig_config_devices) < 0) {
                warn("memory allocation failed: %s", err.msg);
                goto fail;
        }

        /* Allocate space for selecting which devices are available for MIG monitor */
        if (new_devices(&err, dev, &mig_monitor_devices) < 0) {
                warn("memory allocation failed: %s", err.msg);
                goto fail;
        }

        /* Select the visible GPU devices. */
        if (dev->ngpus > 0) {
                if (select_devices(&err, ctx->devices, dev, &devices) < 0) {
                        warnx("device error: %s", err.msg);
                        goto fail;
                }
        }

        /* Select the devices available for MIG config among the visible devices. */
        if (select_mig_config_devices(&err, ctx->mig_config, &devices, &mig_config_devices) < 0) {
                warnx("mig-config error: %s", err.msg);
                goto fail;
        }

        /* Select the devices available for MIG monitor among the visible . */
        if (select_mig_monitor_devices(&err, ctx->mig_monitor, &devices, &mig_monitor_devices) < 0) {
                warnx("mig-monitor error: %s", err.msg);
                goto fail;
        }

        /*
         * Check the container requirements.
         * Try evaluating per visible device first, and globally otherwise.
         */
        for (size_t i = 0; i < devices.ngpus; ++i) {
                struct dsl_data data = {drv, devices.gpus[i]};
                for (size_t j = 0; j < ctx->nreqs; ++j) {
                        if (dsl_evaluate(&err, ctx->reqs[j], &data, rules, nitems(rules)) < 0) {
                                warnx("requirement error: %s", err.msg);
                                goto fail;
                        }
                }
                eval_reqs = false;
        }
        for (size_t i = 0; i < devices.nmigs; ++i) {
                struct dsl_data data = {drv, devices.migs[i]->parent};
                for (size_t j = 0; j < ctx->nreqs; ++j) {
                        if (dsl_evaluate(&err, ctx->reqs[j], &data, rules, nitems(rules)) < 0) {
                                warnx("requirement error: %s", err.msg);
                                goto fail;
                        }
                }
                eval_reqs = false;
        }
        if (eval_reqs) {
                struct dsl_data data = {drv, NULL};
                for (size_t j = 0; j < ctx->nreqs; ++j) {
                        if (dsl_evaluate(&err, ctx->reqs[j], &data, rules, nitems(rules)) < 0) {
                                warnx("requirement error: %s", err.msg);
                                goto fail;
                        }
                }
        }

        /* Mount the driver, visible devices, mig-configs and mig-monitors. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_MOUNT], ecaps_size(NVC_MOUNT)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if (libnvc.driver_mount(nvc, cnt, drv) < 0) {
                warnx("mount error: %s", libnvc.error(nvc));
                goto fail;
        }
        for (size_t i = 0; i < devices.ngpus; ++i) {
                if (libnvc.device_mount(nvc, cnt, devices.gpus[i]) < 0) {
                        warnx("mount error: %s", libnvc.error(nvc));
                        goto fail;
                }
        }
        if (!mig_config_devices.all && !mig_monitor_devices.all) {
                for (size_t i = 0; i < devices.nmigs; ++i) {
                        if (libnvc.mig_device_access_caps_mount(nvc, cnt, devices.migs[i]) < 0) {
                                warnx("mount error: %s", libnvc.error(nvc));
                                goto fail;
                        }
                }
        }
        if (mig_config_devices.all && mig_config_devices.ngpus) {
                if (libnvc.mig_config_global_caps_mount(nvc, cnt) < 0) {
                        warnx("mount error: %s", libnvc.error(nvc));
                        goto fail;
                }
                for (size_t i = 0; i < mig_config_devices.ngpus; ++i) {
                        if (libnvc.device_mig_caps_mount(nvc, cnt, mig_config_devices.gpus[i]) < 0) {
                                warnx("mount error: %s", libnvc.error(nvc));
                                goto fail;
                        }
                }
        }
        if (mig_monitor_devices.all && mig_monitor_devices.ngpus) {
                if (libnvc.mig_monitor_global_caps_mount(nvc, cnt) < 0) {
                        warnx("mount error: %s", libnvc.error(nvc));
                        goto fail;
                }
                for (size_t i = 0; i < mig_monitor_devices.ngpus; ++i) {
                        if (libnvc.device_mig_caps_mount(nvc, cnt, mig_monitor_devices.gpus[i]) < 0) {
                                warnx("mount error: %s", libnvc.error(nvc));
                                goto fail;
                        }
                }
        }

        /* Update the container ldcache. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_LDCACHE], ecaps_size(NVC_LDCACHE)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if (libnvc.ldcache_update(nvc, cnt) < 0) {
                warnx("ldcache error: %s", libnvc.error(nvc));
                goto fail;
        }
        if (libnvc.symlink_libraries(nvc, cnt, drv) < 0) {
                warnx("symlink libraries error: %s", libnvc.error(nvc));
                goto fail;
        }
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_SHUTDOWN], ecaps_size(NVC_SHUTDOWN)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        rv = EXIT_SUCCESS;

 fail:
        free_devices(&devices);
        libnvc.shutdown(nvc);
        libnvc.container_free(cnt);
        libnvc.device_info_free(dev);
        libnvc.driver_info_free(drv);
        libnvc.container_config_free(cnt_cfg);
        libnvc.config_free(nvc_cfg);
        libnvc.context_free(nvc);
        error_reset(&err);
        return (rv);
}
