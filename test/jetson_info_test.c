#include <stdio.h>
#include <criterion/criterion.h>

#include "utils.h"
#include "jetson_info.h"

const char *libs_example[] = { "lib/foo" };
const char *dirs_example[] = { "dir/foo" };
const char *devs_example[] = { "dev/foo" };
const char *sym_source_example[] = { "src/foo" };
const char *sym_target_example[] = { "dst/foo" };

Test(jetson_info_happy, append_empty) {
        struct error err;

        struct nvc_jetson_info a = {0};
        struct nvc_jetson_info b = {0};
        struct nvc_jetson_info *c = jetson_info_append(&err, &a, &b);

        cr_assert(c != NULL);
        cr_assert(c->nlibs == 0);
        cr_assert(c->libs == NULL);

        cr_assert(c->ndirs == 0);
        cr_assert(c->dirs == NULL);

        cr_assert(c->ndevs == 0);
        cr_assert(c->devs == NULL);

        cr_assert(c->nsymlinks == 0);
        cr_assert(c->symlinks_source == NULL);
        cr_assert(c->symlinks_target == NULL);

        free(c);
}

Test(jetson_info_happy, append_a_or_b_empty) {
        struct error err;

        struct nvc_jetson_info a = {0};
        struct nvc_jetson_info b = {
                .libs = (char **) libs_example, .nlibs = 2,
                .dirs = (char **) dirs_example, .ndirs = 1,
                .devs = (char **) devs_example, .ndevs = 1,
                .symlinks_source = (char **) sym_source_example,
                .symlinks_target = (char **) sym_target_example,
                .nsymlinks = 1
        };

        struct {
                struct nvc_jetson_info *a, *b;
                struct nvc_jetson_info *cmp;
        } tests[] = {
                { .a = &a, .b = &b, .cmp = &b },
                { .a = &b, .b = &a, .cmp = &b }
        };

        for (size_t i = 0; i < nitems(tests); ++i) {
                struct nvc_jetson_info *c = jetson_info_append(&err, tests[i].a, tests[i].b);

                cr_assert(c != NULL);
                cr_assert(c->nlibs == tests[i].cmp->nlibs);
                cr_assert(!strcmp(c->libs[0], tests[i].cmp->libs[0]));
                cr_assert(!strcmp(c->libs[1], tests[i].cmp->libs[1]));

                cr_assert(c->ndirs == tests[i].cmp->ndirs);
                cr_assert(!strcmp(c->dirs[0], tests[i].cmp->dirs[0]));

                cr_assert(c->ndevs == tests[i].cmp->ndevs);
                cr_assert(!strcmp(c->devs[0], tests[i].cmp->devs[0]));

                cr_assert(c->nsymlinks == tests[i].cmp->nsymlinks);
                cr_assert(!strcmp(c->symlinks_source[0], tests[i].cmp->symlinks_source[0]));
                cr_assert(!strcmp(c->symlinks_target[0], tests[i].cmp->symlinks_target[0]));

                free(c);
        }
}

Test(jetson_info_happy, append_a_b) {
        struct error err;

        struct nvc_jetson_info a = {
                .libs = (char **) libs_example, .nlibs = 2,
                .dirs = (char **) dirs_example, .ndirs = 1,
                .devs = (char **) devs_example, .ndevs = 1,
                .symlinks_source = (char **) sym_source_example,
                .symlinks_target = (char **) sym_target_example,
                .nsymlinks = 1
        };
        struct nvc_jetson_info b = {
                .libs = (char **) libs_example, .nlibs = 2,
                .dirs = (char **) dirs_example, .ndirs = 1,
                .devs = (char **) devs_example, .ndevs = 1,
                .symlinks_source = (char **) sym_source_example,
                .symlinks_target = (char **) sym_target_example,
                .nsymlinks = 1
        };

        struct nvc_jetson_info *c = jetson_info_append(&err, &a, &b);

        cr_assert(c != NULL);

        cr_assert(c->nlibs == a.nlibs + b.nlibs);
        for (size_t i = 0; i < a.nlibs; ++i)
                cr_assert(!strcmp(c->libs[i], a.libs[i]));
        for (size_t i = 0; i < b.nlibs; ++i)
                cr_assert(!strcmp(c->libs[a.nlibs + i], b.libs[i]));

        cr_assert(c->ndirs == a.ndirs + b.ndirs);
        for (size_t i = 0; i < a.ndirs; ++i)
                cr_assert(!strcmp(c->dirs[i], a.dirs[i]));
        for (size_t i = 0; i < b.ndirs; ++i)
                cr_assert(!strcmp(c->dirs[a.ndirs + i], b.dirs[i]));

        cr_assert(c->ndevs == a.ndevs + b.ndevs);
        for (size_t i = 0; i < a.ndevs; ++i)
                cr_assert(!strcmp(c->devs[i], a.devs[i]));
        for (size_t i = 0; i < b.ndevs; ++i)
                cr_assert(!strcmp(c->devs[a.ndevs + i], b.devs[i]));

        cr_assert(c->nsymlinks == a.nsymlinks + b.nsymlinks);
        for (size_t i = 0; i < a.nsymlinks; ++i) {
                cr_assert(!strcmp(c->symlinks_source[i], a.symlinks_source[i]));
                cr_assert(!strcmp(c->symlinks_target[i], a.symlinks_target[i]));
        }
        for (size_t i = 0; i < b.nsymlinks; ++i) {
                cr_assert(!strcmp(c->symlinks_source[a.nsymlinks + i], b.symlinks_source[i]));
                cr_assert(!strcmp(c->symlinks_target[a.nsymlinks + i], b.symlinks_target[i]));
        }

        free(c);
}

Test(jetson_info_sad, append_a_null) {
        struct error err;

        struct nvc_jetson_info a = {0};

        struct {
                struct nvc_jetson_info *a, *b;
        } tests[] = {
                { .a = &a, .b = NULL },
                { .a = NULL, .b = &a }
        };

        for (size_t i = 0; i < nitems(tests); ++i) {
                struct nvc_jetson_info *c = jetson_info_append(&err, tests[i].a, tests[i].b);
                cr_assert(c == NULL);
        }
}

Test(jetson_info_happy, lookup_nvidia_dir) {
        const char *base = "/nfs/libnvidia-container/test/nvidia_dir";
        size_t baselen = strlen(base) + 1; // Add a separator '/'

        char **files = NULL;
        size_t len = 0;
        struct error err;

        files = jetson_info_lookup_nvidia_dir(&err, base, &len);
        cr_assert(files != NULL);
        cr_assert(len == 3);

        const char *expected_files[] = { "acsv.csv", "bcsv.csv", "zcsv.csv" };
        for (size_t i = 0; i < nitems(expected_files); ++i) {
                for (size_t j = 0; j < len; ++j) {
                        if (!strcmp(expected_files[i], files[j] + baselen))
                                goto nxt;
                }
                cr_assert(0, "Couldn't find %s", expected_files[i]);
nxt:
                continue;
        }

}
