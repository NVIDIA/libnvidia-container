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

static int
resolve_symlink(struct error *err, const char *src, char *dst)
{
        ssize_t n;

        n = readlink(src, dst, PATH_MAX);
        if (n < 0 || n >= PATH_MAX)
                return -1;

        dst[n] = '\0';

        return (0);
}

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
                if (!match_jetson_library_flags(paths[i], cnt->flags) &&
                    !match_jetson_directory_flags(paths[i], cnt->flags))
                        continue;

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

        for (size_t i = 0; i < size; ++i) {
                if (!match_jetson_symlink_flags(paths[i], cnt->flags))
                        continue;

                if (path_new(err, src, root) < 0)
                        return (-1);
                if (path_new(err, dst, cnt->cfg.rootfs) < 0)
                        return (-1);

                if (path_append(err, src, paths[i]) < 0)
                        return (-1);
                if (path_append(err, dst, paths[i]) < 0)
                        return (-1);

                if (resolve_symlink(err, src, src_lnk) < 0)
                        return (-1);

                printf("src: %s, src_lnk: %s, dst: %s, dst_lnk: %s\n", src, src_lnk, dst);
                if (remove(dst) < 0 && errno != ENOENT)
                        return (-1);

                log_infof("symlinking %s to %s", dst, src_lnk);
                if (file_create(err, dst, src_lnk, cnt->uid, cnt->gid, MODE_LNK(0777)) < 0)
                        return (-1);
        }

        return (0);
}
