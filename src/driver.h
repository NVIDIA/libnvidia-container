/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_DRIVER_H
#define HEADER_DRIVER_H

#include <sys/types.h>

#include <stdbool.h>

#include "error.h"
#include "dxcore.h"

struct driver_device;

int driver_init(struct error *, struct dxcore_context *, const char *, uid_t, gid_t);
int driver_shutdown(struct error *);
int driver_get_rm_version(struct error*, char **);
int driver_get_cuda_version(struct error*, char **);
int driver_get_device_count(struct error*, unsigned int *);
int driver_get_device(struct error*, unsigned int, struct driver_device **);
int driver_get_device_minor(struct error*, struct driver_device *, unsigned int *);
int driver_get_device_busid(struct error*, struct driver_device *, char **);
int driver_get_device_uuid(struct error*, struct driver_device *, char **);
int driver_get_device_arch(struct error*, struct driver_device *, char **);
int driver_get_device_model(struct error*, struct driver_device *, char **);
int driver_get_device_brand(struct error*, struct driver_device *, char **);
int driver_get_device_mig_capable(struct error*, struct driver_device *, bool *);
int driver_get_device_mig_enabled(struct error*, struct driver_device *, bool *);
int driver_get_device_max_mig_device_count(struct error*, struct driver_device *, unsigned int *);
int driver_get_device_mig_device(struct error*, struct driver_device *, unsigned int, struct driver_device **);
int driver_get_device_gpu_instance_id(struct error*, struct driver_device *, unsigned int *);
int driver_get_device_compute_instance_id(struct error*, struct driver_device *, unsigned int *);

#endif /* HEADER_DRIVER_H */
