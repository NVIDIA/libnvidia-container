/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <errno.h>
#include <libgen.h>
#undef basename /* Use the GNU version of basename. */
#include <limits.h>
#include <nvidia-modprobe-utils.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "cgroup.h"
#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

static char **mount_files(struct error *, const char *, const struct nvc_container *, const char *, char *[], size_t);
static char **mount_driverstore_files(struct error *, const char *, const struct nvc_container *, const char *, const char *[], size_t);
static char *mount_directory(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_firmware(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_in_root(struct error *err, const char *src, const char *rootfs, const char *path, uid_t uid, uid_t gid, unsigned long mountflags);
static char *mount_with_flags(struct error *, const char *, const char *,  uid_t, uid_t, unsigned long);
static char *mount_device(struct error *, const char *, const struct nvc_container *, const struct nvc_device_node *);
static char *mount_ipc(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_procfs(struct error *, const char *, const struct nvc_container *);
static char *mount_procfs_gpu(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_procfs_mig(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_app_profile(struct error *, const struct nvc_container *);
static int  update_app_profile(struct error *, const struct nvc_container *, dev_t);
static void unmount(const char *);
static int  symlink_library(struct error *, const char *, const char *, const char *, uid_t, gid_t);
static int  symlink_libraries(struct error *, const struct nvc_container *, const char * const [], size_t);
static int  device_mount_dxcore(struct nvc_context *, const struct nvc_container *);
static int  device_mount_native(struct nvc_context *, const struct nvc_container *, const struct nvc_device *);
static int  cap_device_mount(struct nvc_context *, const struct nvc_container *, const char *);
static int  setup_mig_minor_cgroups(struct error *, const struct nvc_container *, int, const struct nvc_device_node *);

static char *
mount_directory(struct error *err, const char *root, const struct nvc_container *cnt, const char *dir)
{
        char src[PATH_MAX];
        if (path_join(err, src, root, dir) < 0)
                return (NULL);
        return mount_in_root(err, src, cnt->cfg.rootfs, dir, cnt->uid, cnt->gid, MS_NOSUID|MS_NOEXEC);
}

// mount_firmware mounts the specified firmware file. The path specified is the container path and is resolved
// on the host before mounting.
static char *
mount_firmware(struct error *err, const char *root, const struct nvc_container *cnt, const char *container_path)
{
        char src[PATH_MAX];
        if (path_resolve_full(err, src, root, container_path) < 0)
                return (NULL);
        return mount_in_root(err, src, cnt->cfg.rootfs, container_path, cnt->uid, cnt->gid, MS_RDONLY|MS_NODEV|MS_NOSUID);
}

// mount_in_root bind mounts the specified src to the specified location in a root.
// If the destination resolves outside of the root an error is raised.
static char *
mount_in_root(struct error *err, const char *src, const char *rootfs, const char *path, uid_t uid, uid_t gid, unsigned long mountflags) {
        char dst[PATH_MAX];
        if (path_resolve_full(err, dst, rootfs, path) < 0)
                return (NULL);
        return mount_with_flags(err, src, dst, uid, gid, mountflags);
}

// mount_with_flags bind mounts the specified src to the specified dst with the specified mount flags
static char *
mount_with_flags(struct error *err, const char *src, const char *dst, uid_t uid, uid_t gid, unsigned long mountflags) {
        mode_t mode;
        char *mnt;

        if (file_mode(err, src, &mode) < 0)
                goto fail;
        if (file_create(err, dst, NULL, uid, gid, mode) < 0)
                goto fail;

        log_infof("mounting %s at %s with flags 0x%lx", src, dst, mountflags);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | mountflags, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(mnt);
        return (NULL);
}

static char **
mount_files(struct error *err, const char *root, const struct nvc_container *cnt, const char *dir, char *paths[], size_t size)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *src_end, *dst_end, *file;
        char **mnt, **ptr;

        if (path_new(err, src, root) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, dir) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, MODE_DIR(0755)) < 0)
                return (NULL);
        if (path_new(err, dst, dir) < 0)
                return (NULL);
        src_end = src + strlen(src);
        dst_end = dst + strlen(dst);

        mnt = ptr = array_new(err, size + 1); /* NULL terminated. */
        if (mnt == NULL)
                return (NULL);

        for (size_t i = 0; i < size; ++i) {
                file = basename(paths[i]);
                if (!match_binary_flags(file, cnt->flags) && !match_library_flags(file, cnt->flags))
                        continue;
                if (path_append(err, src, paths[i]) < 0)
                        goto fail;
                if (file_mode_nofollow(err, src, &mode) < 0)
                        goto fail;
                // If we encounter resolved directories or symlinks here, we raise an error.
                if (S_ISDIR(mode) || S_ISLNK(mode)) {
                        error_setx(err, "unexpected source file mode %o for %s", mode, paths[i]);
                        goto fail;
                }
                if (path_append(err, dst, file) < 0)
                        goto fail;
                if ((*ptr++ = mount_in_root(err, src, cnt->cfg.rootfs, dst, cnt->uid, cnt->gid, MS_RDONLY|MS_NODEV|MS_NOSUID)) == NULL)
                        goto fail;
                *src_end = '\0';
                *dst_end = '\0';
        }
        return (mnt);

 fail:
        for (size_t i = 0; i < size; ++i)
                unmount(mnt[i]);
        array_free(mnt, size);
        return (NULL);
}

static char **
mount_driverstore_files(struct error *err, const char *root, const struct nvc_container *cnt, const char *driverStore, const char *files[], size_t size)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        char *src_end, *dst_end, *file;
        char **mnt, **ptr;

        if (path_join(err, src, root, driverStore) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, driverStore) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, MODE_DIR(0755)) < 0)
                return (NULL);

        src_end = src + strlen(src);
        dst_end = dst + strlen(dst);

        mnt = ptr = array_new(err, size + 1); /* NULL terminated. */
        if (mnt == NULL)
                return (NULL);

        for (size_t i = 0; i < size; ++i) {
                file = basename(files[i]);

                if (path_append(err, src, files[i]) < 0)
                        goto fail;
                if (path_append(err, dst, file) < 0)
                        goto fail;
                if (file_create(err, dst, NULL, cnt->uid, cnt->gid, MODE_REG(0555)) < 0)
                        goto fail;

                log_infof("mounting %s at %s", src, dst);

                if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                        goto fail;
                if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT|MS_RDONLY|MS_NODEV|MS_NOSUID, NULL) < 0)
                        goto fail;
                if ((*ptr++ = xstrdup(err, dst)) == NULL)
                        goto fail;
                *src_end = '\0';
                *dst_end = '\0';
        }

        return (mnt);

 fail:
        for (size_t i = 0; i < size; ++i)
                unmount(mnt[i]);
        array_free(mnt, size);
        return (NULL);
}

static char *
mount_device(struct error *err, const char *root, const struct nvc_container *cnt, const struct nvc_device_node *dev)
{
        struct stat s;
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_join(err, src, root, dev->path) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, dev->path) < 0)
                return (NULL);
        if (xstat(err, src, &s) < 0)
                return (NULL);
        if (s.st_rdev != dev->id) {
                error_setx(err, "invalid device node: %s", src);
                return (NULL);
        }
        if (file_mode(err, src, &mode) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(dst);
        return (NULL);
}

static char *
mount_ipc(struct error *err, const char *root, const struct nvc_container *cnt, const char *ipc)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_join(err, src, root, ipc) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, ipc) < 0)
                return (NULL);
        if (file_mode(err, src, &mode) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(dst);
        return (NULL);
}

static char *
mount_app_profile(struct error *err, const struct nvc_container *cnt)
{
        char path[PATH_MAX];
        char *mnt;

        if (path_resolve_full(err, path, cnt->cfg.rootfs, NV_APP_PROFILE_DIR) < 0)
                return (NULL);
        if (file_create(err, path, NULL, cnt->uid, cnt->gid, MODE_DIR(0555)) < 0)
                return (NULL);

        log_infof("mounting tmpfs at %s", path);
        if (xmount(err, "tmpfs", path, "tmpfs", 0, "mode=0555") < 0)
                goto fail;
        /* XXX Some kernels require MS_BIND in order to remount within a userns */
        if (xmount(err, NULL, path, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, path)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(path);
        return (NULL);
}

static int
update_app_profile(struct error *err, const struct nvc_container *cnt, dev_t id)
{
        char path[PATH_MAX];
        char *buf = NULL;
        char *ptr;
        uintmax_t n;
        uint64_t dev;
        int rv = -1;

#define profile quote_str({\
        "profiles": [{"name": "_container_", "settings": ["EGLVisibleDGPUDevices", 0x%lx]}],\
        "rules": [{"pattern": [], "profile": "_container_"}]\
})

        dev = 1ull << minor(id);
        if (path_resolve_full(err, path, cnt->cfg.rootfs, NV_APP_PROFILE_DIR "/10-container.conf") < 0)
                return (-1);
        if (file_read_text(err, path, &buf) < 0) {
                if (err->code != ENOENT)
                        goto fail;
                if (xasprintf(err, &buf, profile, dev) < 0)
                        goto fail;
        } else {
                if ((ptr = strstr(buf, "0x")) == NULL ||
                    (n = strtoumax(ptr, NULL, 16)) == UINTMAX_MAX) {
                        error_setx(err, "invalid application profile: %s", path);
                        goto fail;
                }
                free(buf), buf = NULL;
                if (xasprintf(err, &buf, profile, (uint64_t)n|dev) < 0)
                        goto fail;
        }
        if (file_create(err, path, buf, cnt->uid, cnt->gid, MODE_REG(0555)) < 0)
                goto fail;
        rv = 0;

#undef profile

 fail:
        free(buf);
        return (rv);
}

static char *
mount_procfs(struct error *err, const char *root, const struct nvc_container *cnt)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        char *src_end, *dst_end, *mnt, *param;
        mode_t mode;
        char *buf = NULL;
        const char *files[] = {
                "params",
                "version",
                "registry",
        };

        if (path_join(err, src, root, NV_PROC_DRIVER) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, NV_PROC_DRIVER) < 0)
                return (NULL);
        src_end = src + strlen(src);
        dst_end = dst + strlen(dst);

        log_infof("mounting tmpfs at %s", dst);
        if (xmount(err, "tmpfs", dst, "tmpfs", 0, "mode=0555") < 0)
                return (NULL);

        for (size_t i = 0; i < nitems(files); ++i) {
                if (path_append(err, src, files[i]) < 0)
                        goto fail;
                if (path_append(err, dst, files[i]) < 0)
                        goto fail;
                if (file_mode(err, src, &mode) < 0) {
                        if (err->code == ENOENT) {
                                log_warnf("%s not found; skipping", src);
                                // We reset the strings to ensure that the paths are
                                // not concatenated if one of the files is not found.
                                *src_end = '\0';
                                *dst_end = '\0';
                                continue;
                        }
                        goto fail;
                }
                if (file_read_text(err, src, &buf) < 0)
                        goto fail;
                /* Prevent NVRM from adjusting the device nodes. */
                if (i == 0 && (param = strstr(buf, "ModifyDeviceFiles: 1")) != NULL)
                        param[19] = '0';
                if (file_create(err, dst, buf, cnt->uid, cnt->gid, mode) < 0)
                        goto fail;
                *src_end = '\0';
                *dst_end = '\0';
                free(buf);
                buf = NULL;
        }
        /* XXX Some kernels require MS_BIND in order to remount within a userns */
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        *dst_end = '\0';
        unmount(dst);
        free(buf);
        return (NULL);
}

static char *
mount_procfs_gpu(struct error *err, const char *root, const struct nvc_container *cnt, const char *busid)
{
        char src[PATH_MAX];
        char dst[PATH_MAX] = {0};
        char *gpu = NULL;
        char *mnt = NULL;
        mode_t mode;

        for (int off = 0;; off += 4) {
                /* XXX Check if the driver procfs uses 32-bit or 16-bit PCI domain */
                if (xasprintf(err, &gpu, "%s/gpus/%s", NV_PROC_DRIVER, busid + off) < 0)
                        return (NULL);
                if (path_join(err, src, root, gpu) < 0)
                        goto fail;
                if (path_resolve_full(err, dst, cnt->cfg.rootfs, gpu) < 0)
                        goto fail;
                if (file_mode(err, src, &mode) == 0)
                        break;
                if (err->code != ENOENT || off != 0)
                        goto fail;
                *dst = '\0';
                free(gpu);
                gpu = NULL;
        }
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                goto fail;

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        free(gpu);
        return (mnt);

 fail:
        free(gpu);
        unmount(dst);
        return (NULL);
}

static char *
mount_procfs_mig(struct error *err, const char *root, const struct nvc_container *cnt, const char *caps_path)
{
        // Initialize local variables.
        char src[PATH_MAX];
        char dst[PATH_MAX] = {0};
        char *mnt = NULL;
        mode_t mode;

        // Set the source path to "<root>/<caps_path>" where 'root' holds the
        // path to root on the host file system, and 'path' holds the path
        // to the MIG capability on the host filesystem.
        if (path_join(err, src, root, caps_path) < 0)
                goto fail;

        // Set the destination path to a similarly named path, but rooted
        // inside the container.
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, caps_path) < 0)
                goto fail;

        // Grab the file mode set on the source path.
        if (file_mode(err, src, &mode) < 0)
                goto fail;

        // Create the destination path with the same mode as the source path.
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                goto fail;

        log_infof("mounting %s at %s", src, dst);

        // Bind mount the source path over the destination path.
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;

        // Remount the destination path to update its mountflags.
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;

        // Copy the destination path out to a newly allocated string and return it.
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;

        return (mnt);

 fail:
        // On failure unmount and filesystem that may have been mounted and return.
        unmount(dst);
        return (NULL);
}

static void
unmount(const char *path)
{
        if (path == NULL || str_empty(path))
                return;
        umount2(path, MNT_DETACH);
        file_remove(NULL, path);
}

static int
symlink_library(struct error *err, const char *src, const char *target, const char *linkname, uid_t uid, gid_t gid)
{
        char path[PATH_MAX];
        char *tmp;
        int rv = -1;

        if ((tmp = xstrdup(err, src)) == NULL)
                return (-1);
        if (path_join(err, path, dirname(tmp), linkname) < 0)
                goto fail;

        log_infof("creating symlink %s -> %s", path, target);
        if (file_create(err, path, target, uid, gid, MODE_LNK(0777)) < 0)
                goto fail;
        rv = 0;

 fail:
        free(tmp);
        return (rv);
}

static int
symlink_libraries(struct error *err, const struct nvc_container *cnt, const char * const paths[], size_t size)
{
        char *lib;

        for (size_t i = 0; i < size; ++i) {
                lib = basename(paths[i]);
                if (str_has_prefix(lib, "libcuda.so")) {
                        /* XXX Many applications wrongly assume that libcuda.so exists (e.g. with dlopen). */
                        if (symlink_library(err, paths[i], SONAME_LIBCUDA, "libcuda.so", cnt->uid, cnt->gid) < 0)
                                return (-1);
                } else if (str_has_prefix(lib, "libGLX_nvidia.so")) {
                        /* XXX GLVND requires this symlink for indirect GLX support. */
                        if (symlink_library(err, paths[i], lib, "libGLX_indirect.so.0", cnt->uid, cnt->gid) < 0)
                                return (-1);
                } else if (str_has_prefix(lib, "libnvidia-opticalflow.so")) {
                        /* XXX Fix missing symlink for libnvidia-opticalflow.so. */
                        if (symlink_library(err, paths[i], "libnvidia-opticalflow.so.1", "libnvidia-opticalflow.so", cnt->uid, cnt->gid) < 0)
                                return (-1);
                }
        }
        return (0);
}

static int
device_mount_dxcore(struct nvc_context *ctx, const struct nvc_container *cnt)
{
        char **drvstore_mnt = NULL;
        size_t drvstore_size = 0;

        // under dxcore we want to mount the driver store key libraries.
        // Devices are not directly visible under dxcore everything is done via /dev/dxg
        // so we only need to mount the per-gpu driver driverStore there are no other per gpu
        // device mounting that needs to be done
        //
        // Note that we are using adapter 0 for all the devices. This is because all
        // the NVIDIA adapters should share the same drivers on a system. If this
        // assumption is changed we will need to query the LUID for each nvc_device
        // and find the matching driver store.
        drvstore_size = (size_t)ctx->dxcore.adapterList[0].driverStoreComponentCount;
        if ((drvstore_mnt = mount_driverstore_files(&ctx->err,
                                                    ctx->cfg.root,
                                                    cnt,
                                                    ctx->dxcore.adapterList[0].pDriverStorePath,
                                                    ctx->dxcore.adapterList[0].pDriverStoreComponents,
                                                    drvstore_size)) == NULL)
        {
                log_errf("failed to mount DriverStore components %s", ctx->dxcore.adapterList[0].pDriverStorePath);
                return (-1);
        }

        return 0;
}

static int
device_mount_native(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        char *dev_mnt = NULL;
        char *proc_mnt = NULL;
        int rv = -1;

        if (!(cnt->flags & OPT_NO_DEVBIND)) {
                if ((dev_mnt = mount_device(&ctx->err, ctx->cfg.root, cnt, &dev->node)) == NULL)
                        goto fail;
        }
        if ((proc_mnt = mount_procfs_gpu(&ctx->err, ctx->cfg.root, cnt, dev->busid)) == NULL)
                goto fail;
        if (cnt->flags & OPT_GRAPHICS_LIBS) {
                if (update_app_profile(&ctx->err, cnt, dev->node.id) < 0)
                        goto fail;
        }
        if (!(cnt->flags & OPT_NO_CGROUPS)) {
                if (setup_device_cgroup(&ctx->err, cnt, dev->node.id) < 0)
                        goto fail;
        }

        rv = 0;

 fail:
        if (rv < 0) {
                unmount(proc_mnt);
                unmount(dev_mnt);
        }

        free(proc_mnt);
        free(dev_mnt);

        return (rv);
}

static int
cap_device_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const char *cap_path)
{
        char *dev_mnt = NULL;
        struct nvc_device_node node = {0};
        int rv = -1;

        if (nvc_nvcaps_device_from_proc_path(ctx, cap_path, &node) < 0)
                goto fail;

        if (!(cnt->flags & OPT_NO_DEVBIND)) {
                if ((dev_mnt = mount_device(&ctx->err, ctx->cfg.root, cnt, &node)) == NULL)
                       goto fail;
        }
        if (!(cnt->flags & OPT_NO_CGROUPS))
                if (setup_device_cgroup(&ctx->err, cnt, node.id) < 0)
                        goto fail;

        rv = 0;

 fail:
        if (rv < 0) {
                unmount(dev_mnt);
        }

        free(node.path);
        free(dev_mnt);

        return (rv);
}

static int
setup_mig_minor_cgroups(struct error *err, const struct nvc_container *cnt, int mig_major, const struct nvc_device_node *node)
{
        unsigned int gpu_minor = 0;
        unsigned int mig_minor = 0;
        char line[PATH_MAX];
        char dummy[PATH_MAX];
        FILE *fp;
        int rv = -1;

        if ((fp = fopen(NV_CAPS_MIG_MINORS_PATH, "r")) == NULL) {
            error_set(err, "unable to open file for reading: %s", NV_CAPS_MIG_MINORS_PATH);
            goto fail;
        }

        line[PATH_MAX - 1] = '\0';
        while (fgets(line, PATH_MAX - 1, fp)) {
                if (sscanf(line, "gpu%u%s %u", &gpu_minor, dummy, &mig_minor) != 3)
                        continue;
                if (gpu_minor != minor(node->id))
                        continue;
                if (setup_device_cgroup(err, cnt, makedev((unsigned int)mig_major, mig_minor)) < 0)
                        goto fail;
        }

        rv = 0;

fail:
        fclose(fp);
        return (rv);
}

int
nvc_driver_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_driver_info *info)
{
        const char **mnt, **ptr, **tmp;
        size_t nmnt;
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && info != NULL) < 0)
                return (-1);

        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        nmnt = 2 + info->nbins + info->nlibs + cnt->nlibs + info->nlibs32 + info->nipcs + info->ndevs + info->nfirmwares;
        mnt = ptr = (const char **)array_new(&ctx->err, nmnt);
        if (mnt == NULL)
                goto fail;

        /* Procfs mount */
        if (ctx->dxcore.initialized)
                log_warn("skipping procfs mount on WSL");
        else if ((*ptr++ = mount_procfs(&ctx->err, ctx->cfg.root, cnt)) == NULL)
                goto fail;

        /* Application profile mount */
        if (cnt->flags & OPT_GRAPHICS_LIBS) {
                if (ctx->dxcore.initialized)
                        log_warn("skipping app profile mount on WSL");
                else if ((*ptr++ = mount_app_profile(&ctx->err, cnt)) == NULL)
                        goto fail;
        }

        /* Host binary and library mounts */
        if (info->bins != NULL && info->nbins > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.bins_dir, info->bins, info->nbins)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if (info->libs != NULL && info->nlibs > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.libs_dir, info->libs, info->nlibs)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if ((cnt->flags & OPT_COMPAT32) && info->libs32 != NULL && info->nlibs32 > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.libs32_dir, info->libs32, info->nlibs32)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if (symlink_libraries(&ctx->err, cnt, mnt, (size_t)(ptr - mnt)) < 0)
                goto fail;

        /* Container library mounts */
        if ((cnt->flags & OPT_CUDA_COMPAT_MODE_MOUNT) && cnt->libs != NULL && cnt->nlibs > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, cnt->cfg.rootfs, cnt, cnt->cfg.libs_dir, cnt->libs, cnt->nlibs)) == NULL) {
                        goto fail;
                }
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }

        /* Firmware mounts */
        for (size_t i = 0; i < info->nfirmwares; ++i) {
                if ((*ptr++ = mount_firmware(&ctx->err, ctx->cfg.root, cnt, info->firmwares[i])) == NULL) {
                        log_errf("error mounting firmware path %s", info->firmwares[i]);
                        goto fail;
                }
        }

        /* IPC mounts */
        for (size_t i = 0; i < info->nipcs; ++i) {
                /* XXX Only utility libraries require persistenced or fabricmanager IPC, everything else is compute only. */
                if (str_has_suffix(NV_PERSISTENCED_SOCKET, info->ipcs[i]) || str_has_suffix(NV_FABRICMANAGER_SOCKET, info->ipcs[i])) {
                        if (!(cnt->flags & OPT_UTILITY_LIBS))
                                continue;
                } else if (!(cnt->flags & OPT_COMPUTE_LIBS))
                        continue;
                if ((*ptr++ = mount_ipc(&ctx->err, ctx->cfg.root, cnt, info->ipcs[i])) == NULL)
                        goto fail;
        }

        /* Device mounts */
        for (size_t i = 0; i < info->ndevs; ++i) {
                /* On WSL2 we only mount the /dev/dxg device and as such these checks are not applicable. */
                if (!ctx->dxcore.initialized) {
                        /* XXX Only compute libraries require specific devices (e.g. UVM). */
                        if (!(cnt->flags & OPT_COMPUTE_LIBS) && major(info->devs[i].id) != NV_DEVICE_MAJOR)
                                continue;
                        /* XXX Only display capability requires the modeset device. */
                        if (!(cnt->flags & OPT_DISPLAY) && minor(info->devs[i].id) == NV_MODESET_DEVICE_MINOR)
                                continue;
                }
                if (!(cnt->flags & OPT_NO_DEVBIND)) {
                        if ((*ptr++ = mount_device(&ctx->err, ctx->cfg.root, cnt, &info->devs[i])) == NULL)
                                goto fail;
                }
                if (!(cnt->flags & OPT_NO_CGROUPS)) {
                        if (setup_device_cgroup(&ctx->err, cnt, info->devs[i].id) < 0)
                                goto fail;
                }
        }
        rv = 0;

 fail:
        if (rv < 0) {
                for (size_t i = 0; mnt != NULL && i < nmnt; ++i)
                        unmount(mnt[i]);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        array_free((char **)mnt, nmnt);
        return (rv);
}

int
nvc_device_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && dev != NULL) < 0)
                return (-1);

        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        if (ctx->dxcore.initialized)
                rv = device_mount_dxcore(ctx, cnt);
        else rv = device_mount_native(ctx, cnt, dev);

        if (rv < 0)
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        else rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);

        return (rv);
}

int
nvc_mig_device_access_caps_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_mig_device *dev)
{
        // Initialize local variables.
        char access[PATH_MAX];
        char *proc_mnt_gi = NULL;
        char *proc_mnt_ci = NULL;
        int rv = -1;

        // Validate incoming arguments.
        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && dev != NULL) < 0)
                return (-1);

        // Enter the mount namespace of the container.
        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        // Construct the path to the 'access' file in '/proc' for the GPU Instance.
        if (path_join(&ctx->err, access, dev->gi_caps_path, NV_MIG_ACCESS_FILE) < 0)
                goto fail;

        // Mount the 'access' file for the GPU Instance into the container.
        if ((proc_mnt_gi = mount_procfs_mig(&ctx->err, ctx->cfg.root, cnt, access)) == NULL)
                goto fail;

        // Check if NV_CAPS_MODULE_NAME exists as a major device,
        // and if so, mount in the /dev based capability as a device.
        if (nvidia_get_chardev_major(NV_CAPS_MODULE_NAME) != -1) {
            if (cap_device_mount(ctx, cnt, access) < 0)
                goto fail;
        }

        // Construct the path to the 'access' file in '/proc' for the Compute Instance.
        if (path_join(&ctx->err, access, dev->ci_caps_path, NV_MIG_ACCESS_FILE) < 0)
                goto fail;

        // Mount the 'access' file for the Compute Instance into the container.
        if ((proc_mnt_ci = mount_procfs_mig(&ctx->err, ctx->cfg.root, cnt, access)) == NULL)
                goto fail;

        // Check if NV_CAPS_MODULE_NAME exists as a major device,
        // and if so, mount in the /dev based capability as a device.
        if (nvidia_get_chardev_major(NV_CAPS_MODULE_NAME) != -1) {
            if (cap_device_mount(ctx, cnt, access) < 0)
                goto fail;
        }

        // Set the return value to indicate success.
        rv = 0;

 fail:
        if (rv < 0) {
                // If we failed above for any reason, unmount the 'access' file
                // we mounted and exit the mount namespace.
                unmount(proc_mnt_gi);
                unmount(proc_mnt_ci);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                // Otherwise, just exit the mount namespace.
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        // In all cases, free the string associated with the mounted 'access'
        // file and return.
        free(proc_mnt_gi);
        free(proc_mnt_ci);
        return (rv);
}

int
nvc_mig_config_global_caps_mount(struct nvc_context *ctx, const struct nvc_container *cnt)
{
        // Initialize local variables.
        char config[PATH_MAX];
        char *dev_mnt = NULL;
        char *proc_mnt = NULL;
        struct nvc_device_node node = {0};
        int rv = -1;

        // Validate incoming arguments.
        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL) < 0)
                return (-1);

        // Enter the mount namespace of the container.
        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        // Mount the entire 'nvidia-capabilities' folder from '/proc' into the container.
        if ((proc_mnt = mount_procfs_mig(&ctx->err, ctx->cfg.root, cnt, NV_PROC_DRIVER_CAPS)) == NULL)
                goto fail;

        // Check if NV_CAPS_MODULE_NAME exists as a major device,
        // and if so, mount in the /dev based capability as a device.
        if (nvidia_get_chardev_major(NV_CAPS_MODULE_NAME) != -1) {
                if ((dev_mnt = mount_directory(&ctx->err, ctx->cfg.root, cnt, NV_CAPS_DEVICE_DIR)) == NULL)
                    goto fail;

                if (path_join(&ctx->err, config, NV_MIG_CAPS_PATH, NV_MIG_CONFIG_FILE) < 0)
                        goto fail;

                if (nvc_nvcaps_device_from_proc_path(ctx, config, &node) < 0)
                        goto fail;

                if (!(cnt->flags & OPT_NO_CGROUPS))
                        if (setup_device_cgroup(&ctx->err, cnt, node.id) < 0)
                                goto fail;
        }

        // Set the return value to indicate success.
        rv = 0;

 fail:
        if (rv < 0) {
                // If we failed above for any reason, unmount the 'access' file
                // we mounted and exit the mount namespace.
                unmount(proc_mnt);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                // Otherwise, just exit the mount namespace.
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        // In all cases, free the string associated with the mounted 'access'
        // file and return.
        free(dev_mnt);
        free(proc_mnt);
        return (rv);
}

int
nvc_mig_monitor_global_caps_mount(struct nvc_context *ctx, const struct nvc_container *cnt)
{
        // Initialize local variables.
        char monitor[PATH_MAX];
        char *dev_mnt = NULL;
        char *proc_mnt = NULL;
        struct nvc_device_node node = {0};
        int rv = -1;

        // Validate incoming arguments.
        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL) < 0)
                return (-1);

        // Enter the mount namespace of the container.
        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        // Mount the entire 'nvidia-capabilities' folder from '/proc' into the container.
        if ((proc_mnt = mount_procfs_mig(&ctx->err, ctx->cfg.root, cnt, NV_PROC_DRIVER_CAPS)) == NULL)
                goto fail;

        // Check if NV_CAPS_MODULE_NAME exists as a major device,
        // and if so, mount in the /dev based capability as a device.
        if (nvidia_get_chardev_major(NV_CAPS_MODULE_NAME) != -1) {
                if ((dev_mnt = mount_directory(&ctx->err, ctx->cfg.root, cnt, NV_CAPS_DEVICE_DIR)) == NULL)
                        goto fail;

                if (path_join(&ctx->err, monitor, NV_MIG_CAPS_PATH, NV_MIG_MONITOR_FILE) < 0)
                        goto fail;

                if (nvc_nvcaps_device_from_proc_path(ctx, monitor, &node) < 0)
                        goto fail;

                if (!(cnt->flags & OPT_NO_CGROUPS))
                        if (setup_device_cgroup(&ctx->err, cnt, node.id) < 0)
                                goto fail;
        }

        // Set the return value to indicate success.
        rv = 0;

 fail:
        if (rv < 0) {
                // If we failed above for any reason, unmount the 'access' file
                // we mounted and exit the mount namespace.
                unmount(proc_mnt);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                // Otherwise, just exit the mount namespace.
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        // In all cases, free the string associated with the mounted 'access'
        // file and return.
        free(dev_mnt);
        free(proc_mnt);
        return (rv);
}

int
nvc_device_mig_caps_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        // Initialize local variables.
        int nvcaps_major = -1;
        int rv = -1;

        // Validate incoming arguments.
        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && dev != NULL) < 0)
                return (-1);

        // Enter the mount namespace of the container.
        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        // Check if NV_CAPS_MODULE_NAME exists as a major device, and if so,
        // mount in the appropriate /dev based capabilities as devices.
        if ((nvcaps_major = nvidia_get_chardev_major(NV_CAPS_MODULE_NAME)) != -1) {
                if (!(cnt->flags & OPT_NO_CGROUPS))
                        if (setup_mig_minor_cgroups(&ctx->err, cnt, nvcaps_major, &dev->node) < 0)
                                goto fail;
        }

        // Set the return value to indicate success.
        rv = 0;

 fail:
        if (rv < 0) {
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        return (rv);
}

int
nvc_imex_channel_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_imex_channel *chan)
{
        // Initialize local variables.
        char path[PATH_MAX];
        struct nvc_device_node node;
        char *mnt = NULL;
        int rv = -1;

        // Validate incoming arguments.
        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && chan != NULL) < 0)
                return (-1);

        // Enter the mount namespace of the container.
        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        // Construct a device node for the channel.
        if (xsnprintf(&ctx->err, path, sizeof(path), NV_CAPS_IMEX_DEVICE_PATH, chan->id) < 0)
            goto fail;
        if (find_device_node(&ctx->err, ctx->cfg.root, path, &node) < 0)
            goto fail;

        // Mount the device node and set up its cgroups (as appropriate).
        if (!(cnt->flags & OPT_NO_DEVBIND)) {
                if ((mnt = mount_device(&ctx->err, ctx->cfg.root, cnt, &node)) == NULL)
                        goto fail;
        }
        if (!(cnt->flags & OPT_NO_CGROUPS)) {
                if (setup_device_cgroup(&ctx->err, cnt, node.id) < 0)
                        goto fail;
        }

        // Set the return value to indicate success.
        rv = 0;

 fail:
        if (rv < 0) {
                unmount(mnt);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }
        free(mnt);

        return (rv);
}
