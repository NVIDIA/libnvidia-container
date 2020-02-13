/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

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

int
select_devices(struct error *err, char *devs, const struct nvc_device *selected[],
    const struct nvc_device available[], size_t size)
{
        char *gpu, *ptr;
        size_t i;
        uintmax_t n;

        while ((gpu = strsep(&devs, ",")) != NULL) {
                if (*gpu == '\0')
                        continue;
                if (str_case_equal(gpu, "all")) {
                        for (i = 0; i < size; ++i)
                                selected[i] = &available[i];
                        break;
                }

                char buf[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE + 1];
                if (!strncasecmp(gpu, "GPU-", strlen("GPU-")) && strlen(gpu) > strlen("GPU-")) {
                        for (i = 0; i < size; ++i) {
                                if (!strncasecmp(available[i].uuid, gpu, strlen(gpu))) {
                                        selected[i] = &available[i];
                                        goto next;
                                }
                        }
                } else if (matches_pci_format(gpu, buf, sizeof(buf))) {
                        for (i = 0; i < size; ++i) {
                                if (!strncasecmp(available[i].busid, buf, strlen(buf))) {
                                        selected[i] = &available[i];
                                        goto next;
                                }
                        }
                } else {
                        n = strtoumax(gpu, &ptr, 10);
                        if (*ptr == '\0' && n < UINTMAX_MAX && (size_t)n < size) {
                                selected[n] = &available[n];
                                goto next;
                        }
                }
                error_setx(err, "unknown device id: %s", gpu);
                return (-1);
         next: ;
        }
        return (0);
}
