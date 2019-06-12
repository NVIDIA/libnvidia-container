/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_CONF_H
#define HEADER_CONF_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "nvc.h"

struct conf_line {
        char *path;
};

struct conf {
        struct error *err;
        const char *path;
        void *base;
        size_t size;

        struct conf_line *lines;
        size_t nlines;
};

void conf_init(struct conf *, struct error *, const char *);
int  conf_open(struct conf *);
int  conf_close(struct conf *);
int  conf_lex(struct conf *);
int  conf_parse(struct conf *, struct nvc_jetson_info *);

#endif /* HEADER_CONF_H */
