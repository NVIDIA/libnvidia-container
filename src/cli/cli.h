/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_CLI_H
#define HEADER_CLI_H

#include <argp.h>
#include <stdbool.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error_generic.h"

struct context;

struct command {
        const char *name;
        const struct argp *argp;
        int (*func)(const struct context *);
};

struct context {
        /* main */
        uid_t uid;
        gid_t gid;
        char *root;
        char *ldcache;
        bool load_kmods;
        bool no_pivot;
        char *init_flags;
        const struct command *command;

        /* info */
        bool csv_output;

        /* configure */
        pid_t pid;
        char *rootfs;
        char *reqs[32];
        size_t nreqs;
        char *ldconfig;
        char *container_flags;

        /* list */
        bool compat32;
        bool list_bins;
        bool list_libs;
        bool list_ipcs;

        char *devices;
        char *mig_config;
        char *mig_monitor;
};

bool matches_pci_format(const char *gpu, char *buf, size_t bufsize);

struct devices {
        bool all;
        const struct nvc_device **gpus;
        size_t max_gpus;
        size_t ngpus;
        const struct nvc_mig_device **migs;
        size_t max_migs;
        size_t nmigs;
};

int new_devices(struct error *err, const struct nvc_device_info *dev, struct devices *d);
void free_devices(struct devices *d);

int print_nvcaps_device_from_proc_file(struct nvc_context *, const char*, const char*);
int print_all_mig_minor_devices(const struct nvc_device_node *);

int select_devices(
    struct error *err,
    char *devs,
    const struct nvc_device_info *available,
    struct devices *selected);

int select_mig_config_devices(
    struct error *err,
    char *devs,
    const struct devices *visible,
    struct devices *selected);

int select_mig_monitor_devices(
    struct error *err,
    char *devs,
    const struct devices *visible,
    struct devices *selected);

extern const struct argp info_usage;
extern const struct argp list_usage;
extern const struct argp configure_usage;

int info_command(const struct context *);
int list_command(const struct context *);
int configure_command(const struct context *);

#endif /* HEADER_CLI_H */
