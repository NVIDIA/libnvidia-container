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
#include <err.h>
#include <libgen.h>
#undef basename /* Use the GNU version of basename. */
#include <stdlib.h>

#include "cli.h"
#include "compat_mode.h"

static void filter_by_major_version(bool, const struct nvc_driver_info *, char * [], size_t *);
static int get_compat_library_path(struct error *, const char * [], size_t, char **);

int
update_compat_libraries(struct nvc_context *ctx, struct nvc_container *cnt, const struct nvc_driver_info *info) {
        if (cnt->flags & OPT_CUDA_COMPAT_MODE_DISABLED) {
                return (0);
        }
        if (cnt->libs == NULL || cnt->nlibs == 0) {
                return (0);
        }
        size_t nlibs = cnt->nlibs;
        char **libs = array_copy(&ctx->err, (const char * const *)cnt->libs, cnt->nlibs);
        if (libs == NULL) {
                return (-1);
        }

        /* For cuda-compat-mode=mount, we also allow compat libraries with a LOWER major versions. */
        bool allow_lower_major_versions = (cnt-> flags & OPT_CUDA_COMPAT_MODE_MOUNT);
        filter_by_major_version(allow_lower_major_versions, info, libs, &nlibs);

        /* Use the filtered library list. */
        free(cnt->libs);
        cnt->libs = libs;
        cnt->nlibs = nlibs;

        if (!(cnt->flags & OPT_CUDA_COMPAT_MODE_LDCONFIG)) {
                return (0);
        }
        /* For cuda-compat-mode=ldconfig we also ensure that cuda_compat_dir is set. */
        if (get_compat_library_path(&ctx->err, (const char **)libs, nlibs, &cnt->cuda_compat_dir) < 0) {
                return (-1);
        }
        return (0);
}

static void
filter_by_major_version(bool allow_lower_major_versions, const struct nvc_driver_info *info, char * paths[], size_t *size)
{
        char *lib, *maj;
        bool exclude;
        /*
         * XXX Filter out any library that has a lower or equal major version than RM to prevent us from
         * running into an unsupported configurations (e.g. CUDA compat on Geforce or non-LTS drivers).
         */
        for (size_t i = 0; i < *size; ++i) {
                lib = basename(paths[i]);
                if ((maj = strstr(lib, ".so.")) != NULL) {
                        maj += strlen(".so.");
                        exclude = false;
                        if (allow_lower_major_versions) {
                                // Only filter out EQUAL RM versions.
                                exclude = (strncmp(info->nvrm_version, maj, strspn(maj, "0123456789")) == 0);
                        } else {
                                // If the major version of RM is greater than or equal to the major version
                                // of the library that we are considering, we remove the library from the
                                // list.
                                exclude = (strncmp(info->nvrm_version, maj, strspn(maj, "0123456789")) >= 0);
                        }
                        if (exclude) {
                                paths[i] = NULL;
                        }
                }
        }
        array_pack(paths, size);
}

static int
get_compat_library_path(struct error *err, const char * paths[], size_t size, char **compat_dir_result)
{
        char *dir;
        char *compat_dir;

        if (size == 0) {
                return 0;
        }

        char **dirnames = array_copy(err, (const char * const *)paths, size);
        if (dirnames == NULL) {
                return -1;
        }

        for (size_t i = 0; i < size; ++i) {
                dir = dirname(dirnames[i]);
                if (i == 0) {
                        compat_dir = strdup(dir);
                        if (compat_dir == NULL) {
                                return -1;
                        }
                        continue;
                }
                if (strcmp(dir, compat_dir)) {
                        goto fail;
                }
        }

        *compat_dir_result = compat_dir;
        return 0;
fail:
        free(dirnames);
        free(compat_dir);
        return -1;
}
