/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>

#include <inttypes.h>
#include <string.h>

#include "cli.h"
#include "nvml.h"

bool
matches_pci_format(const char *gpu, char *buf, size_t bufsize) {
        int domainid, busid, deviceid;

        // the path we get may not have leading zeros
        int out = sscanf(gpu, "%x:%02x:%02x.0", &domainid, &busid, &deviceid);
        if (out != 3) {
                return false;
        }

        // update to expected format with leading zeros
        snprintf(buf, bufsize, "%08x:%02x:%02x.0", domainid, busid, deviceid);
        return true;
}

static size_t
count_mig_devices(const struct nvc_device gpus[], size_t size)
{
        // Walk through all GPUs, and count the total number of MIG devices
        // that exist across all of them. Return that count.
        size_t count = 0;
        for (size_t i = 0; i < size; ++i) {
                count += gpus[i].mig_devices.ndevices;
        }
        return count;
}

static bool
already_present(
    const void *ptr,
    const void *ptrs,
    size_t size)
{
        // Check if a pointer is already an element in a generic pointer array.
        // This function is written generically so it can be reused across an
        // array of pointers to any type.
        for (size_t i = 0; i < size; ++i)
                if (((void**)ptrs)[i] == ptr)
                        return (true);
        return (false);
}

static int
add_gpu_device(
    struct error *err,
    const struct nvc_device *gpu,
    struct devices *devices)
{
        if (!already_present(gpu, devices->gpus, devices->ngpus)) {
                // Make sure we don't go beyond the maximum
                // size of our GPU device array.
                if (devices->ngpus + 1 > devices->max_gpus) {
                        error_setx(err, "exceeds maximum GPU device count");
                        return (-1);
                }
                devices->gpus[devices->ngpus++] = gpu;
        }
        return (0);
}

static int
add_mig_device(
    struct error *err,
    const struct nvc_mig_device *mig,
    struct devices *devices)
{
        if (!already_present(mig, devices->migs, devices->nmigs)) {
                // Make sure we don't go beyond the maximum
                // size of our MIG devices array.
                if (devices->nmigs + 1 > devices->max_migs) {
                        error_setx(err, "exceeds maximum MIG device count");
                        return (-1);
                }
                devices->migs[devices->nmigs++] = mig;
        }
        return (0);
}

static int
select_all_gpu_devices(
    struct error *err,
    const struct nvc_device_info *available,
    struct devices *selected)
{
        // Initialize local variables.
        struct error ierr = {0};
        const struct nvc_device *gpu = NULL;

        // Walk through all GPUs and populate 'devices' as appropriate.
        for (size_t i = 0; i < available->ngpus; ++i) {
                gpu = &available->gpus[i];
                if (add_gpu_device(&ierr, gpu, selected) < 0)
                        goto fail;
        }

        return (0);

 fail:
        error_setx(err, "error adding all GPU devices: %s", ierr.msg);
        error_reset(&ierr);
        return (-1);
}

static int
select_all_mig_devices(
    struct error *err,
    const struct nvc_device_info *available,
    struct devices *selected)
{
        // Initialize local variables.
        struct error ierr = {0};
        const struct nvc_device *gpu = NULL;
        const struct nvc_mig_device *mig = NULL;

        // Walk through all MIG devices and populate 'devices' as appropriate.
        for (size_t i = 0; i < available->ngpus; ++i) {
                gpu = &available->gpus[i];
                for (size_t j = 0; j < gpu->mig_devices.ndevices; ++j) {
                        mig = &gpu->mig_devices.devices[j];
                        if (add_mig_device(&ierr, mig, selected) < 0)
                                goto fail;
                }
        }

        return (0);

 fail:
        error_setx(err, "error adding all MIG devices: %s", ierr.msg);
        error_reset(&ierr);
        return (-1);
}

static int
select_all_devices(
    struct error *err,
    const struct nvc_device_info *available,
    struct devices *selected)
{
        if(select_all_gpu_devices(err, available, selected) < 0)
                return (-1);
        if(select_all_mig_devices(err, available, selected) < 0)
                return (-1);
        return (0);
}

static int
select_gpu_device(
    struct error *err,
    char *dev,
    const struct nvc_device_info *available,
    struct nvc_device **gpu,
    struct devices *selected)
{
        // Initialize local variables.
        struct error ierr = {0};
        char *ptr;
        uintmax_t n;

        // Initialize return values.
        *gpu = NULL;

        // Check if dev matches a full GPU UUID.
        if (!strncasecmp(dev, "GPU-", strlen("GPU-")) && strlen(dev) > strlen("GPU-")) {
                for (size_t i = 0; i < available->ngpus; ++i) {
                        if (strlen(available->gpus[i].uuid) != strlen(dev))
                                continue;
                        if (!strcasecmp(available->gpus[i].uuid, dev)) {
                                *gpu = &available->gpus[i];
                                goto found;
                        }
                }
        }

        // Check if dev matches a PCI bus ID.
        char buf[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE + 1];
        if (matches_pci_format(dev, buf, sizeof(buf))) {
                for (size_t i = 0; i < available->ngpus; ++i) {
                        if (!strncasecmp(available->gpus[i].busid, buf, strlen(buf))) {
                                *gpu = &available->gpus[i];
                                goto found;
                        }
                }
        }

        // Check if dev matches a device index.
        n = strtoumax(dev, &ptr, 10);
        if (*ptr == '\0' && n < UINTMAX_MAX && (size_t)n < available->ngpus) {
                *gpu = &available->gpus[n];
                goto found;
        }

        // If no device was found, just return (without an error).
        return (0);

 found:
        if (add_gpu_device(&ierr, *gpu, selected) < 0) {
                error_setx(err, "error adding GPU device: %s", ierr.msg);
                error_reset(&ierr);
                return (-1);
        }
        return (0);
}

static int
select_mig_device(
    struct error *err,
    char *dev,
    const struct nvc_device_info *available,
    struct nvc_mig_device **mig,
    struct devices *selected)
{
        // Initialize local variables.
        size_t i, j;
        struct error ierr = {0};
        const struct nvc_device *gpu = NULL;

        // Initialize return values.
        *mig = NULL;

        // Check if dev matches a full MIG device UUID.
        if (!strncasecmp(dev, "MIG-", strlen("MIG-")) && strlen(dev) > strlen("MIG-")) {
                for (i = 0; i < available->ngpus; ++i) {
                        gpu = &available->gpus[i];
                        for (j = 0; j < gpu->mig_devices.ndevices; ++j) {
                                if (strlen(gpu->mig_devices.devices[j].uuid) != strlen(dev))
                                        continue;
                                if (!strcasecmp(gpu->mig_devices.devices[j].uuid, dev)) {
                                        *mig = &gpu->mig_devices.devices[j];
                                        goto found;
                                }
                        }
                }
        }

        // Check if a dev matched a MIG index in the format
        // <GPU_idx>:<MIG_idx>.
        if (sscanf(dev, "%lu:%lu", &i, &j) == 2) {
           if (i < available->ngpus) {
                   gpu = &available->gpus[i];
                   if (j < gpu->mig_devices.ndevices) {
                           *mig = &gpu->mig_devices.devices[j];
                           goto found;
                   }
           }
        }

        // If no device was found, just return (without an error).
        return (0);

 found:
        if (add_mig_device(&ierr, *mig, selected) < 0) {
                error_setx(err, "error adding MIG device: %s", ierr.msg);
                error_reset(&ierr);
                return (-1);
        }
        if (add_gpu_device(&ierr, (*mig)->parent, selected) < 0) {
                error_setx(err, "error adding GPU device: %s", ierr.msg);
                error_reset(&ierr);
                return (-1);
        }
        return (0);
}

int
select_devices(
    struct error *err,
    char *devs,
    const struct nvc_device_info *gpus,
    struct devices *selected)
{
        // Initialize local variables.
        char sep[2] = ",";
        struct error ierr = {0};
        char *dev = NULL;
        struct nvc_device *gpu = NULL;
        struct nvc_mig_device *mig = NULL;

        // Walk trough the comma separated device string and populate
        // 'selected' devices from it.
        while ((dev = strsep(&devs, sep)) != NULL) {
                // Allow extra commas between device strings.
                if (*dev == '\0')
                        continue;

                // Special case the "all" string to select all devices.
                if (str_case_equal(dev, "all")) {
                        if (select_all_devices(&ierr, gpus, selected) < 0)
                                goto fail;
                        selected->all = true;
                        break;
                }

                // Attempt to select a GPU device from the device string. If it is
                // already present in the array of GPU devices, don't add it again.
                // Always continue to the next 'dev' if one is found.
                if (select_gpu_device(&ierr, dev, gpus, &gpu, selected) < 0)
                        goto fail;
                if (gpu != NULL)
                        continue;

                // Attempt to select a MIG device from the device string. If it is
                // already present in the array of MIG devices, don't add it again.
                // Always continue to the next 'dev' if one is found.
                if (select_mig_device(&ierr, dev, gpus, &mig, selected) < 0)
                        goto fail;
                if (mig != NULL)
                        continue;

                // If 'dev' has not been captured by the time we get here, it
                // is an error.
                error_setx(&ierr, "unknown device");
                goto fail;
        }

        return (0);

 fail:
        error_setx(err, "%s: %s", dev, ierr.msg);
        error_reset(&ierr);
        return (-1);
}

int
select_mig_config_devices(
    struct error *err,
    char *devs,
    const struct devices *visible,
    struct devices *selected)
{
        // Initialize local variables.
        char sep[2] = ",";
        struct error ierr = {0};
        char *dev = NULL;

        // Walk trough the comma separated device string and populate
        // 'selected' devices from it.
        while ((dev = strsep(&devs, sep)) != NULL) {
                // Allow extra commas between device strings.
                if (*dev == '\0')
                        continue;

                // For now, only support the "all" string to select all visible devices.
                if (str_case_equal(dev, "all")) {
                        if (!visible->all && visible->nmigs > 0) {
                                error_setx(&ierr, "cannot enable mig-config with specific MIG devices selected");
                                goto fail;
                        }
                        for (size_t i = 0; i < visible->ngpus; i++) {
                                if (!visible->gpus[i]->mig_capable)
                                        continue;
                                if (add_gpu_device(&ierr, visible->gpus[i], selected) < 0)
                                        goto fail;
                        }
                        for (size_t i = 0; i < visible->nmigs; i++) {
                                if (!visible->migs[i]->parent->mig_capable)
                                        continue;
                                if(add_gpu_device(&ierr, visible->migs[i]->parent, selected) < 0)
                                        goto fail;
                        }
                        selected->all = true;
                        break;
                }

                // If 'dev' has not been captured by the time we get here, it
                // is an error.
                error_setx(&ierr, "only 'all' devices are currently supported");
                goto fail;
        }

        return (0);

 fail:
        error_setx(err, "%s: %s", dev, ierr.msg);
        error_reset(&ierr);
        return (-1);
}

int
select_mig_monitor_devices(
    struct error *err,
    char *devs,
    const struct devices *visible,
    struct devices *selected)
{
        // Initialize local variables.
        char sep[2] = ",";
        struct error ierr = {0};
        char *dev = NULL;

        // Walk trough the comma separated device string and populate
        // 'selected' devices from it.
        while ((dev = strsep(&devs, sep)) != NULL) {
                // Allow extra commas between device strings.
                if (*dev == '\0')
                        continue;

                // For now, only support the "all" string to select all visible devices.
                if (str_case_equal(dev, "all")) {
                        if (!visible->all && visible->nmigs > 0) {
                                error_setx(&ierr, "cannot enable mig-monitor with specific MIG devices selected");
                                goto fail;
                        }
                        for (size_t i = 0; i < visible->ngpus; i++) {
                                if (!visible->gpus[i]->mig_capable)
                                        continue;
                                if (add_gpu_device(&ierr, visible->gpus[i], selected) < 0)
                                        goto fail;
                        }
                        for (size_t i = 0; i < visible->nmigs; i++) {
                                if (!visible->migs[i]->parent->mig_capable)
                                        continue;
                                if(add_gpu_device(&ierr, visible->migs[i]->parent, selected) < 0)
                                        goto fail;
                        }
                        selected->all = true;
                        break;
                }

                // If 'dev' has not been captured by the time we get here, it
                // is an error.
                error_setx(&ierr, "only 'all' devices are currently supported");
                goto fail;
        }

        return (0);

 fail:
        error_setx(err, "%s: %s", dev, ierr.msg);
        error_reset(&ierr);
        return (-1);
}

int
new_devices(struct error *err, const struct nvc_device_info *dev, struct devices *d)
{
        // Allocate space for all of the elements in a 'devices' struct and
        // initialize the memory of the struct itself to 0.
        memset(d, 0, sizeof(*d));
        d->max_gpus = dev->ngpus;
        d->max_migs = count_mig_devices(dev->gpus, dev->ngpus);
        if ((d->gpus = xcalloc(err, d->max_gpus, sizeof(*d->gpus))) == NULL ||
            (d->migs = xcalloc(err, d->max_migs, sizeof(*d->migs))) == NULL) {
                return (-1);
        }
        return (0);
}

void
free_devices(struct devices *d)
{
        // Free all space allocated for the elements in a 'devices' struct
        // and reinitialize the memory of the struct itself to 0.
        free(d->gpus);
        free(d->migs);
        memset(d, 0, sizeof(*d));
}

int
print_nvcaps_device_from_proc_file(struct nvc_context *ctx, const char* cap_dir, const char* cap_file)
{
        char cap_path[PATH_MAX];
        struct nvc_device_node node;

        if (path_join(NULL, cap_path, cap_dir, cap_file) < 0)
                return (-1);
        if (nvc_nvcaps_device_from_proc_path(ctx, cap_path, &node) < 0)
                return (-1);

        printf("%s\n", node.path);
        free(node.path);

        return (0);
}

int
print_all_mig_minor_devices(const struct nvc_device_node *node)
{
        unsigned int gpu_minor = 0;
        unsigned int mig_minor = 0;
        char line[PATH_MAX];
        char dummy[PATH_MAX];
        FILE *fp;
        int rv = -1;

        if ((fp = fopen(NV_CAPS_MIG_MINORS_PATH, "r")) == NULL) {
            goto fail;
        }

        line[PATH_MAX - 1] = '\0';
        while (fgets(line, PATH_MAX - 1, fp)) {
                if (sscanf(line, "gpu%u%s %u", &gpu_minor, dummy, &mig_minor) != 3)
                        continue;
                if (gpu_minor != minor(node->id))
                        continue;
                printf(NV_CAPS_DEVICE_PATH "\n", mig_minor);
        }

        rv = 0;

fail:
        fclose(fp);
        return (rv);
}
