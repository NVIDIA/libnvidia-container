/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_CSV_H
#define HEADER_CSV_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "nvc.h"

#define CSV_TOKEN_LIB "lib"
#define CSV_TOKEN_DIR "dir"
#define CSV_TOKEN_DEV "dev"
#define CSV_TOKEN_SYM "symlink"

struct csv_line {
        char **tokens;
        size_t ntokens;
};

struct csv {
        struct error *err;
        const char *path;
        void *base;
        size_t size;

        struct csv_line *lines;
        size_t nlines;
};

void csv_init(struct csv *, struct error *, const char *);
int  csv_open(struct csv *);
int  csv_close(struct csv *);
int  csv_lex(struct csv *);
int  csv_parse(struct csv *, struct nvc_jetson_info *);

#endif /* HEADER_CSV_H */
