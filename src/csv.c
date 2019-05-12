/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/mman.h>

#include <limits.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "csv.h"
#include "utils.h"
#include "xfuncs.h"
#include "jetson_info.h"

#define MAX_NUM_FIELDS_PER_LINE 3

#if !(defined(TESTING) && TESTING == TRUE)
# define printf(...)
#endif

static void csv_pack(struct csv *);
static void trim(char **);

void
csv_init(struct csv *ctx, struct error *err, const char *path)
{
        *ctx = (struct csv){err, path, NULL, 0, NULL, 0};
}

int
csv_open(struct csv *ctx)
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
csv_close(struct csv *ctx)
{
        if (file_unmap(ctx->err, ctx->path, ctx->base, ctx->size) < 0)
                return (-1);

        ctx->base = NULL;
        ctx->size = 0;
        return (0);
}

void
csv_pack(struct csv *ctx)
{
        size_t idx = 0;

        if (ctx->lines == NULL)
                return;

        for (size_t ptr = 0; ptr < ctx->nlines; ++ptr) {
                if (ctx->lines[ptr].ntokens == 0)
                        continue;

                // Evict empty lines
                if (ctx->lines[ptr].ntokens == 1 && ctx->lines[ptr].tokens[0][0] == '\0') {
                        free(ctx->lines[ptr].tokens);
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
csv_lex(struct csv *ctx)
{
        size_t file_len = 0;
        size_t line_len = 0;
        size_t ntokens = 0;

        char *ptr = ctx->base;
        char *file_end = ((char *) ctx->base) + ctx->size;

        ctx->nlines = str_count(ptr, '\n', ctx->size);
        ctx->lines = xcalloc(ctx->err, ctx->nlines, sizeof(struct csv_line));
        if (ctx->lines == NULL)
                return (-1);

        printf("Number of lines: %lu\n", ctx->nlines);

        // Each iteration matches parsing a line
        // ntoken = number of commas + 1
        // We NULL terminated the file as part of the open step, which allows us to use strsep
        // We aren't using array_new here because the table of string contains mmaped value
        //    hence these can't be freed by array_free.
        for (size_t line = 0; line < ctx->nlines; ++line) {
                file_len = (size_t) (file_end - ptr);
                line_len = str_ncspn(ptr, '\n', file_len);
                ntokens = str_count(ptr, ',', line_len) + 1;

                ctx->lines[line].ntokens = ntokens;
                ctx->lines[line].tokens = xcalloc(ctx->err, ntokens, sizeof(char **));
                if (ctx->lines[line].tokens == NULL)
                        return (-1);

                printf("[%lu] line_len: %lu\n", line, line_len);
                printf("[%lu] ntokens: %lu\n", line, ntokens);
                printf("[%lu] file_len: %lu\n", line, file_len);

                for (size_t i = 0; i < ntokens; ++i) {
                        ctx->lines[line].tokens[i] = strsep(&ptr, ",\n");
                        trim(&ctx->lines[line].tokens[i]);

                        printf("[%lu][%lu] token: '%s'\n", line, i, ctx->lines[line].tokens[i]);
                }
        }

        printf("packing\n");
        csv_pack(ctx);
        printf("finished packing\n");

        return (0);
}

int
csv_parse(struct csv *ctx, struct nvc_jetson_info *info)
{
        struct csv_line line;

        if (jetson_info_init(ctx->err, info, ctx->nlines) < 0)
                return (-1);

        for (size_t i = 0; i < ctx->nlines; ++i) {
                line = ctx->lines[i];
                if (line.ntokens > 1)
                        continue;

                error_setx(ctx->err, "malformed line %lu, expected at least 2 tokens", i);
                return (-1);
        }

        for (size_t i = 0; i < ctx->nlines; ++i) {
                line = ctx->lines[i];

                if (!strcmp(line.tokens[0], CSV_TOKEN_LIB)) {
                        if (line.ntokens != 2) {
                                error_setx(ctx->err, "malformed line %lu, expected 2 tokens", i);
                                return (-1);
                        }

                        info->libs[i] = xstrdup(ctx->err, line.tokens[1]);
                        if (info->libs[i] == NULL)
                                return (-1);

                        printf("[%lu] lib: '%s'\n", i, info->libs[i]);
                } else if (!strcmp(line.tokens[0], CSV_TOKEN_DIR)) {
                        if (line.ntokens != 2) {
                                error_setx(ctx->err, "malformed line %lu, expected 2 tokens", i);
                                return (-1);
                        }

                        info->dirs[i] = xstrdup(ctx->err, line.tokens[1]);
                        if (info->dirs[i] == NULL)
                                return (-1);

                        printf("[%lu] dir: '%s'\n", i, info->dirs[i]);
                } else if (!strcmp(line.tokens[0], CSV_TOKEN_DEV)) {
                        if (line.ntokens != 2) {
                                error_setx(ctx->err, "malformed line %lu, expected 2 tokens", i);
                                return (-1);
                        }

                        info->devs[i] = xstrdup(ctx->err, line.tokens[1]);
                        if (info->devs[i] == NULL)
                                return (-1);

                        printf("[%lu] dev: '%s'\n", i, info->devs[i]);
                } else if (!strcmp(line.tokens[0], CSV_TOKEN_SYM)) {
                        if (line.ntokens != 3) {
                                error_setx(ctx->err, "malformed line %lu, expected 3 tokens", i);
                                return (-1);
                        }

                        info->symlinks_source[i] = xstrdup(ctx->err, line.tokens[1]);
                        if (info->symlinks_source[i] == NULL)
                                return (-1);

                        info->symlinks_target[i] = xstrdup(ctx->err, line.tokens[2]);
                        if (info->symlinks_target[i] == NULL)
                                return (-1);

                        printf("[%lu] symlink: source: '%s', dest: '%s'\n", i,
                                        info->symlinks_source[i],
                                        info->symlinks_target[i]);
                } else {
                        error_setx(ctx->err, "malformed line %lu, unexpected symbol '%s'", i,
                                        line.tokens[0]);
                        return (-1);
                }
        }

        jetson_info_pack(info, ctx->nlines);

        return (0);
}
