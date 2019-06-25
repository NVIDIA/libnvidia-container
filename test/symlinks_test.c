#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <ftw.h>

#include <criterion/criterion.h>

#include "utils.h"
#include "jetson_mount.h"

static int
remove_file(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
        (void) sb;
        (void) tflag;
        (void) ftwbuf;
        if (remove(fpath) < 0) {
                printf("Failed removing %s with errno: %d\n", fpath, errno);
                return (-1);
        }

        return (0);
}

static int
recursive_remove(const char *path)
{
    return nftw(path, remove_file, 20, FTW_DEPTH | FTW_PHYS);
}


#define SRC_FOLDER "symlink_tests_src"
#define DST_FOLDER "symlink_tests_dst"

/*
Test(jetson_resolve_symlink, relative_link) {
        struct error err;
        char src[] = SRC_FOLDER "/foo";
        char dst[] = "../" DST_FOLDER "/bar";

        cr_assert(recursive_remove(SRC_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(mkdir(SRC_FOLDER, 0700) >= 0);

        cr_assert(symlink(dst, src) >= 0);

}*/

Test(create_jetson_symlinks, happy_absolute_links) {
        char src[PATH_MAX], src_lnk[PATH_MAX], dst[PATH_MAX];
        char *paths[] = { src };

        char buf[PATH_MAX];
        char cwd[PATH_MAX];
        char rootfs[PATH_MAX];
        struct error err;
        struct nvc_container cnt;
        ssize_t n;

        /* Cleanup and setup */
        cr_assert(getcwd(cwd, PATH_MAX) != NULL);

        cr_assert(recursive_remove("./" SRC_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove("./" DST_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(mkdir("./" SRC_FOLDER, 0700) >= 0);
        cr_assert(mkdir("./" DST_FOLDER, 0700) >= 0);

        cr_assert(path_new(&err, src, cwd) >= 0);
        cr_assert(path_new(&err, src_lnk, cwd) >= 0);
        cr_assert(path_append(&err, src, SRC_FOLDER) >= 0);
        cr_assert(path_append(&err, src_lnk, SRC_FOLDER) >= 0);
        cr_assert(path_append(&err, src, "foo") >= 0);
        cr_assert(path_append(&err, src_lnk, "bar") >= 0);

        sprintf(rootfs, "%s/%s", cwd, DST_FOLDER);
        cnt.uid = getuid();
        cnt.gid = getgid();
        cnt.cfg.rootfs = rootfs;

        printf("src: %s, src_lnk: %s, rootfs: %s\n", src, src_lnk, rootfs);

        cr_assert(symlink(src_lnk, src) >= 0);
        /* end setup */

        cr_assert(create_jetson_symlinks(&err, "/", &cnt, paths, 1) >= 0);

        cr_assert(path_new(&err, dst, cwd) >= 0);
        cr_assert(path_append(&err, dst, DST_FOLDER) >= 0);
        cr_assert(path_append(&err, dst, src) >= 0);

        n = readlink(dst, buf, PATH_MAX);
        cr_assert(n > 0 && n < PATH_MAX);
        buf[n] = '\0';

        printf("buf: %s, dst: %s, dst_lnk: %s\n", buf, dst, src_lnk);
        cr_assert(strcmp(buf, src_lnk) == 0);

        /* Cleanup */
        cr_assert(recursive_remove("./" SRC_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove("./" DST_FOLDER) >= 0 || errno == ENOENT);
}

Test(create_jetson_symlinks, happy_relative_links) {
        char src_lnk[PATH_MAX] = "../bar";
        char src[PATH_MAX], dst[PATH_MAX];
        char *paths[] = { src };

        char buf[PATH_MAX];
        char cwd[PATH_MAX];
        char rootfs[PATH_MAX];
        struct error err;
        struct nvc_container cnt;
        ssize_t n;

        /* Cleanup and setup */
        cr_assert(getcwd(cwd, PATH_MAX) != NULL);

        cr_assert(recursive_remove("./" SRC_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove("./" DST_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(mkdir("./" SRC_FOLDER, 0700) >= 0);
        cr_assert(mkdir("./" DST_FOLDER, 0700) >= 0);

        cr_assert(path_new(&err, src, cwd) >= 0);
        cr_assert(path_append(&err, src, SRC_FOLDER) >= 0);
        cr_assert(path_append(&err, src, "foo") >= 0);

        sprintf(rootfs, "%s/%s", cwd, DST_FOLDER);
        cnt.uid = getuid();
        cnt.gid = getgid();
        cnt.cfg.rootfs = rootfs;

        printf("src: %s, src_lnk: %s, rootfs: %s\n", src, src_lnk, rootfs);

        cr_assert(symlink(src_lnk, src) >= 0);
        /* end setup */

        cr_assert(create_jetson_symlinks(&err, "/", &cnt, paths, 1) >= 0);

        cr_assert(path_new(&err, dst, cwd) >= 0);
        cr_assert(path_append(&err, dst, DST_FOLDER) >= 0);
        cr_assert(path_append(&err, dst, src) >= 0);

        n = readlink(dst, buf, PATH_MAX);
        cr_assert(n > 0 && n < PATH_MAX);
        buf[n] = '\0';

        printf("buf: %s, dst: %s, dst_lnk: %s\n", buf, dst, src_lnk);
        cr_assert(strcmp(buf, src_lnk) == 0);

        /* Cleanup */
        cr_assert(recursive_remove("./" SRC_FOLDER) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove("./" DST_FOLDER) >= 0 || errno == ENOENT);
}
