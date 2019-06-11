/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_CSV_H
#define HEADER_CSV_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "nvc.h"

struct csv_line {
        char *path;
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
