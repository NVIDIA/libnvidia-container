/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#include <dirent.h>
#include <string.h>

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
                *init[i].len = len;
                *init[i].arr = array_new(err, len);
                if (*init[i].arr == NULL) {
                        return (-1);
                }
        }

        return (0);
}

void
jetson_info_free(struct nvc_jetson_info *info)
{
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

void
jetson_info_pack(struct nvc_jetson_info *info, size_t max_len)
{
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
                *init[i].len = max_size;
        }
}

struct nvc_jetson_info *
jetson_info_append(struct error *err, struct nvc_jetson_info *a, struct nvc_jetson_info *b)
{
        struct nvc_jetson_info *info = NULL;
        char **arr, **a_arr, **b_arr;
        size_t a_len, b_len;

        if (a == NULL || b == NULL) {
                return NULL;
        }

        if ((info = xcalloc(err, 1, sizeof(struct nvc_jetson_info))) == NULL)
                return NULL;

        struct {
                char **a_arr;
                size_t a_len;
                char **b_arr;
                size_t b_len;
                char ***arr;
                size_t *len;
        } init[] = {
                { a->libs, a->nlibs, b->libs, b->nlibs, &info->libs, &info->nlibs },
                { a->dirs, a->ndirs, b->dirs, b->ndirs, &info->dirs, &info->ndirs },
                { a->devs, a->ndevs, b->devs, b->ndevs, &info->devs, &info->ndevs },
                { a->symlinks_source, a->nsymlinks, b->symlinks_source, b->nsymlinks, &info->symlinks_source, &info->nsymlinks },
                { a->symlinks_target, a->nsymlinks, b->symlinks_target, b->nsymlinks, &info->symlinks_target, &info->nsymlinks }
        };


        for (size_t i = 0; i < nitems(init); ++i) {
                a_len = init[i].a_len;
                b_len = init[i].b_len;
                a_arr = init[i].a_arr;
                b_arr = init[i].b_arr;

                *init[i].len = a_len + b_len;
                if (*init[i].len == 0)
                        continue;

                if ((arr = array_new(err, *init[i].len)) == NULL)
                        return NULL;

                arr = array_new(err, *init[i].len);
                if (arr == NULL) {
                        return NULL;
                }

                *init[i].arr = arr;
                for (size_t j = 0; j < a_len; ++j) {
                        if ((arr[j] = xstrdup(err, a_arr[j])) == NULL)
                                return NULL;
                }

                for (size_t j = 0; j < b_len; ++j) {
                        if ((arr[a_len + j] = xstrdup(err, b_arr[j])) == NULL)
                                return NULL;
                }
        }

        return info;
}

char **
jetson_info_lookup_nvidia_dir(struct error *err, const char *base, size_t *len)
{
        DIR *d;
        struct dirent *dir;
        size_t dlen = 0, dnamelen = 0;
        char **rv = NULL;
        char path[PATH_MAX];

        *len = 0;

        if ((d = opendir(base)) == NULL) {
                error_set(err, "open failed: %s", base);
                return NULL;
        }

        while ((dir = readdir(d)) != NULL)
                ++dlen;

        rewinddir(d);

        if ((rv = array_new(err, dlen)) == NULL)
                goto fail;

        while ((dir = readdir(d)) != NULL) {
                if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
                        continue;

                dnamelen = strlen(dir->d_name);
                if (dnamelen < 4)
                        continue;

                if (strcmp(dir->d_name + dnamelen - 4, ".csv"))
                        continue;

                strcpy(path, base);
                strcat(path, "/");
                strcat(path, dir->d_name);

                rv[*len] = xstrdup(err, path);
                if (rv[*len] == NULL) {
                        free(rv);
                        rv = NULL;
                        goto fail;
                }

                ++(*len);
        }

fail:
        closedir(d);

        return (rv);
}
