#include <stdio.h>
#include <criterion/criterion.h>
#include <csv.h>

Test(csv_happy, lex_simple) {
        struct csv ctx;
        struct error err;

        csv_init(&ctx, &err, "./test/csv_samples/simple.csv");
        csv_open(&ctx);

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

        cr_assert(ctx.lines[3].ntokens == 3);
        cr_assert(!strcmp(ctx.lines[3].tokens[0], "symlink"));
        cr_assert(!strcmp(ctx.lines[3].tokens[1], "/source"));
        cr_assert(!strcmp(ctx.lines[3].tokens[2], "/target"));

        csv_close(&ctx);
}

Test(csv_happy, parse_simple) {
        struct csv ctx;
        struct error err;
        struct nvc_jetson_info info;

        csv_init(&ctx, &err, "./test/csv_samples/simple.csv");
        csv_open(&ctx);

        cr_assert(csv_lex(&ctx) == 0);
        cr_assert(csv_parse(&ctx, &info) == 0);

        cr_assert(info.nlibs == 1);
        cr_assert(!strcmp(info.libs[0], "/lib/target"));

        cr_assert(info.ndirs == 1);
        cr_assert(!strcmp(info.dirs[0], "/lib/target"));

        cr_assert(info.ndevs == 1);
        cr_assert(!strcmp(info.devs[0], "/dev/target"));

        cr_assert(info.nsymlinks == 1);
        cr_assert(!strcmp(info.symlinks_source[0], "/source"));
        cr_assert(!strcmp(info.symlinks_target[0], "/target"));

        csv_close(&ctx);
}


Test(csv_happy, lex_spaced) {
        struct csv ctx;
        struct error err;

        csv_init(&ctx, &err, "./test/csv_samples/spaced.csv");
        csv_open(&ctx);

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

        cr_assert(ctx.lines[3].ntokens == 3);
        cr_assert(!strcmp(ctx.lines[3].tokens[0], "symlink"));
        cr_assert(!strcmp(ctx.lines[3].tokens[1], "/source"));
        cr_assert(!strcmp(ctx.lines[3].tokens[2], "/target"));

        csv_close(&ctx);
}

Test(csv_sad, parse_simple) {
    cr_assert(1);
}
