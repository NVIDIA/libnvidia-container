#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <errno.h>
#include <libgen.h>
#undef basename /* Use the GNU version of basename. */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"
#include "jetson_mount.h"

char **
mount_jetson_files(struct error *err, const char *root, const struct nvc_container *cnt, char *paths[], size_t size)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char **mnt, **ptr;

        mnt = ptr = array_new(err, size + 1); /* NULL terminated. */
        if (mnt == NULL)
                return (NULL);

        for (size_t i = 0; i < size; ++i) {
                if (path_new(err, src, root) < 0)
                        goto fail;
                if (path_new(err, dst, cnt->cfg.rootfs) < 0)
                        goto fail;

                if (path_append(err, src, paths[i]) < 0)
                        goto fail;
                if (path_append(err, dst, paths[i]) < 0)
                        goto fail;

                if (file_mode(err, src, &mode) < 0)
                        goto fail;
                if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                        goto fail;

                log_infof("mounting %s at %s", src, dst);
                if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                        goto fail;
                if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID, NULL) < 0)
                        goto fail;

                if ((*ptr++ = xstrdup(err, dst)) == NULL)
                        goto fail;
        }
        return (mnt);

 fail:
        for (size_t i = 0; i < size; ++i)
                unmount(mnt[i]);
        array_free(mnt, size);
        return (NULL);
}

int
create_jetson_symlinks(struct error *err, const char *root, const struct nvc_container *cnt, char *paths[], size_t size)
{
        char src[PATH_MAX];
        char src_lnk[PATH_MAX];
        char dst[PATH_MAX];
        char dst_lnk[PATH_MAX];

        for (size_t i = 0; i < size; ++i) {
                if (path_new(err, src, root) < 0)
                        return (-1);
                if (path_new(err, dst, cnt->cfg.rootfs) < 0)
                        return (-1);
                if (path_new(err, dst_lnk, cnt->cfg.rootfs) < 0)
                        return (-1);

                if (path_append(err, src, paths[i]) < 0)
                        return (-1);
                if (path_append(err, dst, paths[i]) < 0)
                        return (-1);
                printf("src: %s, dst: %s\n", src, dst);

                if (resolve_next_symlink(err, src, src_lnk) < 0)
                        return (-1);
                if (path_append(err, dst_lnk, src_lnk) < 0)
                        return (-1);

                if (remove(dst) < 0 && errno != ENOENT)
                        return (-1);

                log_infof("symlinking %s to %s", dst, dst_lnk);
                if (file_create(err, dst, dst_lnk, cnt->uid, cnt->gid, S_IFLNK) < 0)
                        return (-1);
        }

        return (0);
}

int resolve_next_symlink(struct error *err, const char *src, char *dst) {
        char buf[PATH_MAX];
        char lnk[PATH_MAX];
        ssize_t n;

        if (path_new(err, lnk, src) < 0)
                return (-1);

        n = readlink(lnk, buf, PATH_MAX);
        if (n < 0 || n >= PATH_MAX)
                return -1;

        buf[n] = '\0';

        if (buf[0] != '/') {
                if (path_new(err, dst, dirname(lnk)) < 0)
                        return (-1);
        } else if (path_new(err, dst, "") < 0)
                return (-1);

        if (path_append(err, dst, buf + strspn(buf, "./")) < 0)
                return (-1);

        return (0);
}
