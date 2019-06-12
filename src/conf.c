/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/mman.h>

#include <limits.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "utils.h"
#include "xfuncs.h"
#include "jetson_info.h"

#define MAX_NUM_FIELDS_PER_LINE 3

#if !(defined(TESTING) && TESTING == TRUE)
# define printf(...)
#endif

static void conf_pack(struct conf *);
static void trim(char **);

void
conf_init(struct conf *ctx, struct error *err, const char *path)
{
        *ctx = (struct conf){err, path, NULL, 0, NULL, 0};
}

int
conf_open(struct conf *ctx)
{
        ctx->base = file_map_prot(ctx->err, ctx->path, &ctx->size, PROT_READ | PROT_WRITE);
        if (ctx->base == NULL)
                return (-1);

        ctx->base = mremap(ctx->base, ctx->size, ctx->size + 1, MREMAP_MAYMOVE);
        if (ctx->base == NULL)
                return (-1);

        ((char *) ctx->base)[ctx->size] = '\0';
        ++ctx->size;

        return (0);
}


int
conf_close(struct conf *ctx)
{
        if (file_unmap(ctx->err, ctx->path, ctx->base, ctx->size) < 0)
                return (-1);

        ctx->base = NULL;
        ctx->size = 0;
        return (0);
}

void
conf_pack(struct conf *ctx)
{
        size_t idx = 0;

        if (ctx->lines == NULL)
                return;

        for (size_t ptr = 0; ptr < ctx->nlines; ++ptr) {
                // Evict empty lines
                if (strlen(ctx->lines[ptr]) == 0) {
                        continue;
                }

                ctx->lines[idx] = ctx->lines[ptr];
                ++idx;
        }

        ctx->nlines = idx;
}

void
trim(char **strp)
{
        // left trim
        char *str = *strp;
        while (*str != '\0' && *str == ' ')
                str++;

        *strp = str;

        // right trim
        str += strcspn(str, " ");
        *str = '\0';
}

int
conf_lex(struct conf *ctx)
{
        char *ptr = ctx->base;
        ctx->nlines = str_count(ptr, '\n', ctx->size);
        ctx->lines = xcalloc(ctx->err, ctx->nlines, sizeof(char *));
        if (ctx->lines == NULL)
                return (-1);

        printf("Number of lines: %lu\n", ctx->nlines);

        // Each iteration matches parsing a line
        // ntoken = number of commas + 1
        // We NULL terminated the file as part of the open step, which allows us to use strsep
        // We aren't using array_new here because the table of string contains mmaped value
        //    hence these can't be freed by array_free.
        for (size_t line = 0; line < ctx->nlines; ++line) {
                ctx->lines[line] = strsep(&ptr, "\n");
		trim(&ctx->lines[line]);

                printf("[%lu] path: '%s'\n", line, ctx->lines[line]);
        }

        printf("packing\n");
        conf_pack(ctx);
        printf("finished packing\n");

        return (0);
}

int
conf_parse(struct conf *ctx, struct nvc_jetson_info *info)
{
        char *line;

        if (jetson_info_init(ctx->err, info, ctx->nlines) < 0)
                return (-1);

        for (size_t i = 0; i < ctx->nlines; ++i) {
                line = ctx->lines[i];

		mode_t mode;
		if (file_mode(ctx->err, line, &mode) < 0)
			continue;

		if (S_ISREG(mode)) {
			info->libs[i] = xstrdup(ctx->err, line);
			if (info->libs[i] == NULL)
				return (-1);

			printf("[%lu] lib: '%s'\n", i, info->libs[i]);
		} else if (S_ISDIR(mode)) {
			info->dirs[i] = xstrdup(ctx->err, line);
			if (info->dirs[i] == NULL)
				return (-1);

			printf("[%lu] dir: '%s'\n", i, info->dirs[i]);
		} else if (S_ISBLK(mode) || S_ISCHR(mode)) {
			info->devs[i] = xstrdup(ctx->err, line);
			if (info->devs[i] == NULL)
				return (-1);

			printf("[%lu] dev: '%s'\n", i, info->devs[i]);
		} else if (S_ISLNK(mode)) {
			info->syms[i] = xstrdup(ctx->err, line);
			if (info->syms[i] == NULL)
				return (-1);

			printf("[%lu] symlink: '%s'\n", i, info->syms[i]);
		} else {
			log_infof("malformed line: %s", line);
			continue;
		}
        }

        jetson_info_pack(info, ctx->nlines);

        return (0);
}
