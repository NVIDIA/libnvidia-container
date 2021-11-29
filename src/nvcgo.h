/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HEADER_NVCGO_H
#define HEADER_NVCGO_H

#include <nvcgo/nvcgo.h>

#include "error.h"
#include "rpc.h"

struct libnvcgo {
        __typeof__(GetDeviceCGroupVersion)   *GetDeviceCGroupVersion;
        __typeof__(GetDeviceCGroupMountPath) *GetDeviceCGroupMountPath;
        __typeof__(GetDeviceCGroupRootPath)  *GetDeviceCGroupRootPath;
        __typeof__(AddDeviceRules)           *AddDeviceRules;
};

struct nvcgo {
        struct rpc rpc;
        struct libnvcgo api;
};

int nvcgo_init(struct error *);
int nvcgo_shutdown(struct error *);
struct nvcgo *nvcgo_get_context(void);

#endif /* HEADER_NVCGO_H */
