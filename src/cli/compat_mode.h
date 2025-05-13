/**
 # SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: Apache-2.0
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 #     http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 **/

#ifndef HEADER_COMPAT_MODE_H
#define HEADER_COMPAT_MODE_H

// TODO: These are duplicated from options.h to prevent conflicts with the CLI
// options header.
enum {
    /* OPT_CUDA_COMPAT_MODE_DISABLED replaced OPT_NO_CNTLIBS. */
    OPT_CUDA_COMPAT_MODE_DISABLED = 1 << 14,
    OPT_CUDA_COMPAT_MODE_LDCONFIG = 1 << 15,
    OPT_CUDA_COMPAT_MODE_MOUNT    = 1 << 16,
};

int update_compat_libraries(struct nvc_context *, struct nvc_container *, const struct nvc_driver_info *);


#endif /* HEADER_COMPAT_MODE_H */
