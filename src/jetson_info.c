/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#include "jetson_info.h"
#include "utils.h"

int
jetson_info_init(struct error *err, struct nvc_jetson_info *info, size_t len)
{
        struct {
                char ***arr;
                size_t *len;
        } init[] = {
                { &info->libs, &info->nlibs },
                { &info->dirs, &info->ndirs },
                { &info->devs, &info->ndevs },
                { &info->symlinks_source, &info->nsymlinks },
                { &info->symlinks_target, &info->nsymlinks }
        };

        for (size_t i = 0; i < nitems(init); ++i) {
                *(init[i].len) = len;
                *(init[i].arr) = array_new(err, len);
                if (*(init[i]).arr == NULL) {
                        return (-1);
                }
        }

        return (0);
}

void
jetson_info_free(struct nvc_jetson_info *info) {
        struct {
                char **arr;
                size_t len;
        } init[] = {
                { info->libs, info->nlibs },
                { info->dirs, info->ndirs },
                { info->devs, info->ndevs },
                { info->symlinks_source, info->nsymlinks },
                { info->symlinks_target, info->nsymlinks }
        };

        for (size_t i = 0; i < nitems(init); ++i) {
                array_free(init[i].arr, init[i].len);
        }
}

void jetson_info_pack(struct nvc_jetson_info *info, size_t max_len) {
        size_t max_size;

        struct {
                char **arr;
                size_t *len;
        } init[] = {
                { info->libs, &info->nlibs },
                { info->dirs, &info->ndirs },
                { info->devs, &info->ndevs },
                { info->symlinks_source, &info->nsymlinks },
                { info->symlinks_target, &info->nsymlinks }
        };


        for (size_t i = 0; i < nitems(init); ++i) {
                max_size = max_len;

                array_pack(init[i].arr, &max_size);
                *(init[i].len) = max_size;
        }
}
