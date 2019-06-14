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


Test(jetson_resolve_symlink, happy_link) {
        struct error err;
        char link[][PATH_MAX] = {"./foo-symlink", "/tmp/foo-symlink" };
        char dst[][PATH_MAX] = {"./bar-symlink", "/tmp/bar-symlink" };

        char buf[PATH_MAX];
        char cwd[PATH_MAX];
        char dst_lnk[PATH_MAX];
        char src_lnk[PATH_MAX];

        for (size_t i = 0; i < nitems(link); ++i) {
                printf("i: %lu\n", i);

                if (link[i][0] != '/')
                        cr_assert(getcwd(cwd, PATH_MAX) != NULL);
                else
                        memset(cwd, '\0', PATH_MAX);

                cr_assert((remove(link[i]) >= 0) || errno == ENOENT);
                cr_assert(symlink(dst[i], link[i]) >= 0);

                // Ignore "./" by adding + 2
                cr_assert(path_new(&err, src_lnk, cwd) >= 0);
                cr_assert(path_append(&err, src_lnk, link[i] + strspn(link[i], "./")) >= 0);

                cr_assert(path_new(&err, dst_lnk, cwd) >= 0);
                cr_assert(path_append(&err, dst_lnk, dst[i]  + strspn(dst[i], "./")) >= 0);

                printf("src_lnk: %s\n", src_lnk);
                printf("dst_lnk: %s\n", dst_lnk);
                cr_assert(resolve_next_symlink(&err, src_lnk, buf) >= 0);
                printf("buf: %s\n", buf);
                cr_assert(strcmp(buf, dst_lnk) == 0);
                cr_assert(remove(link[i]) >= 0);
        }
}

Test(create_jetson_symlinks, happy_link) {
        char test_dir_src[] = "./symlink_tests_src";
        char test_dir_dst[] = "./symlink_tests_dst";

        char src[PATH_MAX], src_lnk[PATH_MAX], dst[PATH_MAX], dst_lnk[PATH_MAX];
        char src_dir[PATH_MAX], dst_dir[PATH_MAX];
        char *paths[] = { src };
        char buf[PATH_MAX];
        char cwd[PATH_MAX];
        struct error err;
        struct nvc_container cnt;
        ssize_t n;

        /* Cleanup and setup */
        cr_assert(recursive_remove(test_dir_src) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove(test_dir_dst) >= 0 || errno == ENOENT);
        cr_assert(mkdir(test_dir_src, 0700) >= 0);
        cr_assert(mkdir(test_dir_dst, 0700) >= 0);

        cr_assert(getcwd(cwd, PATH_MAX) != NULL);
        cr_assert(path_new(&err, src_dir, cwd) >= 0);
        cr_assert(path_new(&err, dst_dir, cwd) >= 0);
        cr_assert(path_new(&err, src, cwd) >= 0);
        cr_assert(path_new(&err, src_lnk, cwd) >= 0);
        cr_assert(path_new(&err, dst, cwd) >= 0);
        cr_assert(path_new(&err, dst_lnk, cwd) >= 0);

        cr_assert(path_append(&err, src_dir, test_dir_src + 2) >= 0);
        cr_assert(path_append(&err, dst_dir, test_dir_dst + 2) >= 0);
        cr_assert(path_append(&err, src, test_dir_src + 2) >= 0);
        cr_assert(path_append(&err, src_lnk, test_dir_src + 2) >= 0);
        cr_assert(path_append(&err, dst, test_dir_dst + 2) >= 0);
        cr_assert(path_append(&err, dst_lnk, test_dir_dst + 2) >= 0);

        cr_assert(path_append(&err, src, "foo") >= 0);
        cr_assert(path_append(&err, src_lnk, "bar") >= 0);
        cr_assert(path_append(&err, dst, src) >= 0);
        cr_assert(path_append(&err, dst_lnk, src_lnk) >= 0);

        printf("src: %s, src_lnk: %s, dst: %s, dst_lnk: %s\n", src, src_lnk, dst, dst_lnk);

        cnt.uid = getuid();
        cnt.gid = getgid();
        cnt.cfg.rootfs = dst_dir;

        cr_assert(symlink(src_lnk, src) >= 0);
        cr_assert(create_jetson_symlinks(&err, "/", &cnt, paths, 1) >= 0);

        n = readlink(dst, buf, PATH_MAX);
        cr_assert(n > 0 && n < PATH_MAX);
        buf[n] = '\0';

        printf("buf: %s, dst_lnk: %s\n", buf, dst);
        cr_assert(strcmp(buf, dst_lnk) == 0);

        /* Cleanup */
        cr_assert(recursive_remove(test_dir_src) >= 0 || errno == ENOENT);
        cr_assert(recursive_remove(test_dir_dst) >= 0 || errno == ENOENT);
}
