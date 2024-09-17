/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <alloca.h>
#include <err.h>
#include <stdio.h>

#include "cli.h"

static error_t list_parser(int, char *, struct argp_state *);

const struct argp list_usage = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"device", 'd', "ID", 0, "Device UUID(s) or index(es) to list", -1},
                {"libraries", 'l', NULL, 0, "List driver libraries", -1},
                {"binaries", 'b', NULL, 0, "List driver binaries", -1},
                {"ipcs", 'i', NULL, 0, "List driver ipcs", -1},
                {"firmwares", 'f', NULL, 0, "List driver firmwares", -1},
                {"compat32", 0x80, NULL, 0, "Enable 32bits compatibility", -1},
                {"mig-config", 0x81, "ID", 0, "MIG devices to list config capabilities files for", -1},
                {"mig-monitor", 0x82, "ID", 0, "MIG devices to list monitor capabilities files for", -1},
                {"imex-channel", 0x83, "CHANNEL", 0, "IMEX channel ID(s) to inject", -1},
                {"no-persistenced", 0x84, NULL, 0, "Don't include the NVIDIA persistenced socket", -1},
                {"no-fabricmanager", 0x85, NULL, 0, "Don't include the NVIDIA fabricmanager socket", -1},
                {0},
        },
        list_parser,
        NULL,
        "Query the driver and list the components required in order to configure a container with GPU support.",
        NULL,
        NULL,
        NULL,
};

static error_t
list_parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'd':
                if (str_join(&err, &ctx->devices, arg, ",") < 0)
                        goto fatal;
                break;
        case 'l':
                ctx->list_libs = true;
                break;
        case 'b':
                ctx->list_bins = true;
                break;
        case 'i':
                ctx->list_ipcs = true;
                break;
        case 'f':
                ctx->list_firmwares = true;
                break;
        case 0x80:
                ctx->compat32 = true;
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
                if (str_join(&err, &ctx->imex_channels, arg, ",") < 0)
                        goto fatal;
                break;
        case 0x84:
                if (str_join(&err, &ctx->driver_opts, "no-persistenced", " ") < 0)
                        goto fatal;
                break;
        case 0x85:
                if (str_join(&err, &ctx->driver_opts, "no-fabricmanager", " ") < 0)
                        goto fatal;
                break;
        case ARGP_KEY_END:
                if (state->argc == 1 || (state->argc == 2 && ctx->imex_channels != NULL)) {
                        if ((ctx->devices = xstrdup(&err, "all")) == NULL)
                                goto fatal;
                        ctx->mig_config = NULL;
                        ctx->mig_monitor = NULL;
                        ctx->compat32 = true;
                        ctx->list_libs = true;
                        ctx->list_bins = true;
                        ctx->list_ipcs = true;
                        ctx->list_firmwares = true;
                }
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);

 fatal:
        errx(EXIT_FAILURE, "input error: %s", err.msg);
        return (0);
}

int
list_command(const struct context *ctx)
{
        bool run_as_root;
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        struct devices devices = {0};
        struct devices mig_config_devices = {0};
        struct devices mig_monitor_devices = {0};
        struct error err = {0};
        int rv = EXIT_FAILURE;

        run_as_root = (geteuid() == 0);
        if (run_as_root) {
                if (perm_set_capabilities(&err, CAP_PERMITTED, pcaps, nitems(pcaps)) < 0 ||
                    perm_set_capabilities(&err, CAP_INHERITABLE, NULL, 0) < 0 ||
                    perm_set_bounds(&err, bcaps, nitems(bcaps)) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        }

        /* Initialize the library context. */
        int c = ctx->load_kmods ? NVC_INIT_KMODS : NVC_INIT;
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[c], ecaps_size(c)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((nvc = libnvc.context_new()) == NULL ||
            (nvc_cfg = libnvc.config_new()) == NULL) {
                warn("memory allocation failed");
                goto fail;
        }
        nvc_cfg->uid = (!run_as_root && ctx->uid == (uid_t)-1) ? geteuid() : ctx->uid;
        nvc_cfg->gid = (!run_as_root && ctx->gid == (gid_t)-1) ? getegid() : ctx->gid;
        nvc_cfg->root = ctx->root;
        nvc_cfg->ldcache = ctx->ldcache;
        if (parse_imex_info(&err, ctx->imex_channels, &nvc_cfg->imex) < 0) {
                warnx("error parsing IMEX info: %s", err.msg);
                goto fail;
        }
        if (libnvc.init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", libnvc.error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_INFO], ecaps_size(NVC_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = libnvc.driver_info_new(nvc, ctx->driver_opts)) == NULL ||
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

        /* Select the devices available for MIG monitor among the visible devices. */
        if (select_mig_monitor_devices(&err, ctx->mig_monitor, &devices, &mig_monitor_devices) < 0) {
                warnx("mig-monitor error: %s", err.msg);
                goto fail;
        }

        /* List the visible GPU devices and MIG devices. */
        if (ctx->devices != NULL) {
                for (size_t i = 0; i < drv->ndevs; ++i) {
                        if (drv->devs[i].path != NULL)
                                printf("%s\n", drv->devs[i].path);
                }
                for (size_t i = 0; i < devices.ngpus; ++i) {
                        if (devices.gpus[i]->node.path != NULL)
                                printf("%s\n", devices.gpus[i]->node.path);
                }
                if (!mig_config_devices.all && !mig_monitor_devices.all) {
                        for (size_t i = 0; i < devices.nmigs; ++i) {
                                printf("%s/%s\n", devices.migs[i]->gi_caps_path, NV_MIG_ACCESS_FILE);
                                printf("%s/%s\n", devices.migs[i]->ci_caps_path, NV_MIG_ACCESS_FILE);
                                if (libnvc.nvcaps_style() == NVC_NVCAPS_STYLE_DEV) {
                                        print_nvcaps_device_from_proc_file(nvc, devices.migs[i]->gi_caps_path, NV_MIG_ACCESS_FILE);
                                        print_nvcaps_device_from_proc_file(nvc, devices.migs[i]->ci_caps_path, NV_MIG_ACCESS_FILE);
                                }
                        }
                }
        }

        /* List the IMEX channel devices. */
        if (ctx->imex_channels != NULL) {
                for (size_t i = 0; i < nvc_cfg->imex.nchans; ++i) {
                        printf(NV_CAPS_IMEX_DEVICE_PATH"\n", nvc_cfg->imex.chans[i].id);
                }
        }

        /* List the files required for MIG configuration of the visible devices */
        if (mig_config_devices.all && mig_config_devices.ngpus) {
                printf("%s/%s\n", NV_MIG_CAPS_PATH, NV_MIG_CONFIG_FILE);
                if (libnvc.nvcaps_style() == NVC_NVCAPS_STYLE_DEV)
                        print_nvcaps_device_from_proc_file(nvc, NV_MIG_CAPS_PATH, NV_MIG_CONFIG_FILE);
                for (size_t i = 0; i < mig_config_devices.ngpus; ++i) {
                        printf("%s\n", mig_config_devices.gpus[i]->mig_caps_path);
                        if (libnvc.nvcaps_style() == NVC_NVCAPS_STYLE_DEV) {
                                printf("%s\n", NV_CAPS_DEVICE_DIR);
                                print_all_mig_minor_devices(&mig_config_devices.gpus[i]->node);
                        }
                }
        }
        /* List the files required for MIG monitoring of the visible devices */
        if (mig_monitor_devices.all && mig_monitor_devices.ngpus) {
                printf("%s/%s\n", NV_MIG_CAPS_PATH, NV_MIG_MONITOR_FILE);
                if (libnvc.nvcaps_style() == NVC_NVCAPS_STYLE_DEV)
                        print_nvcaps_device_from_proc_file(nvc, NV_MIG_CAPS_PATH, NV_MIG_MONITOR_FILE);
                for (size_t i = 0; i < mig_monitor_devices.ngpus; ++i) {
                        printf("%s\n", mig_monitor_devices.gpus[i]->mig_caps_path);
                        if (libnvc.nvcaps_style() == NVC_NVCAPS_STYLE_DEV) {
                                printf("%s\n", NV_CAPS_DEVICE_DIR);
                                print_all_mig_minor_devices(&mig_monitor_devices.gpus[i]->node);
                        }
                }
        }

        /* List the driver devices */
        if (ctx->list_bins) {
                for (size_t i = 0; i < drv->nbins; ++i)
                        printf("%s\n", drv->bins[i]);
        }
        if (ctx->list_libs) {
                for (size_t i = 0; i < drv->nlibs; ++i)
                        printf("%s\n", drv->libs[i]);
                if (ctx->compat32) {
                        for (size_t i = 0; i < drv->nlibs32; ++i)
                                printf("%s\n", drv->libs32[i]);
                }
        }
        if (ctx->list_ipcs) {
                for (size_t i = 0; i < drv->nipcs; ++i)
                        printf("%s\n", drv->ipcs[i]);
        }
        if (ctx->list_firmwares) {
                for (size_t i = 0; i < drv->nfirmwares; ++i)
                        printf("%s\n", drv->firmwares[i]);
        }

        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_SHUTDOWN], ecaps_size(NVC_SHUTDOWN)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        rv = EXIT_SUCCESS;
 fail:
        free(nvc_cfg->imex.chans);
        free_devices(&devices);
        libnvc.shutdown(nvc);
        libnvc.device_info_free(dev);
        libnvc.driver_info_free(drv);
        libnvc.config_free(nvc_cfg);
        libnvc.context_free(nvc);
        error_reset(&err);
        return (rv);
}
