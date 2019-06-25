#include <stdio.h>
#include <criterion/criterion.h>

#include "csv.h"
#include "jetson_info.h"

Test(csv_happy, lex_simple) {
        struct csv ctx;
        struct error err;

        csv_init(&ctx, &err, "./test/csv_samples/simple.csv");
        cr_assert(csv_open(&ctx) == 0);

        cr_assert(csv_lex(&ctx) == 0);

        cr_assert(ctx.nlines == 4);

        cr_assert(ctx.lines[0].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[0].tokens[0], "lib"));
        cr_assert(!strcmp(ctx.lines[0].tokens[1], "/lib/target"));

        cr_assert(ctx.lines[1].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[1].tokens[0], "dir"));
        cr_assert(!strcmp(ctx.lines[1].tokens[1], "/lib/target"));

        cr_assert(ctx.lines[2].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[2].tokens[0], "dev"));
        cr_assert(!strcmp(ctx.lines[2].tokens[1], "/dev/target"));

        cr_assert(ctx.lines[3].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[3].tokens[0], "sym"));
        cr_assert(!strcmp(ctx.lines[3].tokens[1], "/source"));

        csv_close(&ctx);
}

Test(csv_happy, parse_simple) {
        struct csv ctx;
        struct error err = {0};
        struct nvc_jetson_info info = {0};

        csv_init(&ctx, &err, "./test/csv_samples/simple.csv");
        cr_assert(csv_open(&ctx) == 0);

        cr_assert(csv_lex(&ctx) == 0);
        cr_assert(csv_parse(&ctx, &info) == 0);

        cr_assert(info.nlibs == 1);
        cr_assert(!strcmp(info.libs[0], "/lib/target"));

        cr_assert(info.ndirs == 1);
        cr_assert(!strcmp(info.dirs[0], "/lib/target"));

        cr_assert(info.ndevs == 1);
        cr_assert(!strcmp(info.devs[0], "/dev/target"));

        cr_assert(info.nsyms == 1);
        cr_assert(!strcmp(info.syms[0], "/source"));

        csv_close(&ctx);
        jetson_info_free(&info);
}

Test(csv_happy, lex_spaced) {
        struct csv ctx;
        struct error err = {0};

        csv_init(&ctx, &err, "./test/csv_samples/spaced.csv");
        cr_assert(csv_open(&ctx) == 0);

        cr_assert(csv_lex(&ctx) == 0);

        cr_assert(ctx.nlines == 4);

        cr_assert(ctx.lines[0].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[0].tokens[0], "dev"));
        cr_assert(!strcmp(ctx.lines[0].tokens[1], "/dev/target"));

        cr_assert(ctx.lines[1].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[1].tokens[0], "lib"));
        cr_assert(!strcmp(ctx.lines[1].tokens[1], "/lib/target"));

        cr_assert(ctx.lines[2].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[2].tokens[0], "dir"));
        cr_assert(!strcmp(ctx.lines[2].tokens[1], "/lib/target"));

        cr_assert(ctx.lines[3].ntokens == 2);
        cr_assert(!strcmp(ctx.lines[3].tokens[0], "sym"));
        cr_assert(!strcmp(ctx.lines[3].tokens[1], "/source"));

        csv_close(&ctx);
}

Test(csv_sad, parse_simple) {
        struct csv ctx;
        struct error err = {0};
        struct nvc_jetson_info info = {0};

        csv_init(&ctx, &err, "./test/csv_samples/simple_wrong.csv");
        cr_assert(csv_open(&ctx) == 0);

        cr_assert(csv_lex(&ctx) == 0);
        cr_assert(csv_parse(&ctx, &info) != 0);

        csv_close(&ctx);
        jetson_info_free(&info);
}

Test(csv_sad, file_does_not_exist) {
        struct csv ctx;
        struct error err = {0};

        csv_init(&ctx, &err, "./NOT-A-CSV.json");
        cr_assert(csv_open(&ctx) != 0);
}
