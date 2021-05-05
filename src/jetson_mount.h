/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_JETSON_MOUNT_H
#define HEADER_JETSON_MOUNT_H

#include "nvc_internal.h"
#include "error.h"

char **mount_jetson_files(struct error *, const char *, const struct nvc_container *, char * [], size_t);
int create_jetson_symlinks(struct error *, const char *, const struct nvc_container *, char * [], size_t);

#endif /* HEADER_JETSON_MOUNT_H */
