/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_ELFTOOL_H
#define HEADER_ELFTOOL_H

#include <stdint.h>

#include <libelf.h>

#include "error.h"

typedef struct {
        uint32_t        n_namesz;    /* Length of note's name. */
        uint32_t        n_descsz;    /* Length of note's value. */
        uint32_t        n_type;      /* Type of note. */
} Elf_Note;


struct elftool {
    struct error *err;
    int fd;
    Elf *elf;
    const char *path;
};

void elftool_init(struct elftool *, struct error *);
int  elftool_open(struct elftool *, const char *);
void elftool_close(struct elftool *);
int  elftool_has_dependency(struct elftool *, const char *);
int  elftool_has_abi(struct elftool *, uint32_t [3]);

#endif /* HEADER_ELFTOOL_H */
