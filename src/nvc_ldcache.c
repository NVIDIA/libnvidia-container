/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/securebits.h>
#include <linux/types.h>

#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <sched.h>
#ifdef WITH_SECCOMP
#include <seccomp.h>
#endif /* WITH_SECCOMP */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

static inline bool secure_mode(void);
static pid_t create_process(struct error *, int);
static int   change_rootfs(struct error *, const char *, bool, bool, uid_t, gid_t, bool *);
static int   adjust_capabilities(struct error *, uid_t, bool);
static int   adjust_privileges(struct error *, uid_t, gid_t, bool);
static int   limit_resources(struct error *);
static int   limit_syscalls(struct error *);
static ssize_t   sendfile_nointr(int, int, off_t *, size_t);
static int       open_as_memfd(struct error *, const char *);
int memfd_create(const char *, unsigned int);


static inline bool
secure_mode(void)
{
        char *s;

        s = secure_getenv("NVC_INSECURE_MODE");
        return (s == NULL || str_equal(s, "0") || str_case_equal(s, "false") || str_case_equal(s, "no"));
}

static pid_t
create_process(struct error *err, int flags)
{
        pid_t child;
        int fd[2] = {-1, -1};
        int null = -1;
        int rv = -1;

        if ((log_active() && pipe(fd) < 0) ||
            (child = (pid_t)syscall(SYS_clone, SIGCHLD|flags, NULL, NULL, NULL, NULL)) < 0) {
                error_set(err, "process creation failed");
                xclose(fd[0]);
                xclose(fd[1]);
                return (-1);
        }

        if (child == 0) {
                if ((null = xopen(err, _PATH_DEVNULL, O_RDWR)) < 0)
                        goto fail;
                if (dup2(null, STDIN_FILENO) < 0 ||
                    dup2(log_active() ? fd[1] : null, STDOUT_FILENO) < 0 ||
                    dup2(log_active() ? fd[1] : null, STDERR_FILENO) < 0) {
                        error_set(err, "file duplication failed");
                        goto fail;
                }
        } else {
                if (log_pipe_output(err, fd) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0) {
                log_errf("could not capture process output: %s", err->msg);
                error_reset(err);
        }
        xclose(fd[0]);
        xclose(fd[1]);
        xclose(null);
        return (child);
}

static int
change_rootfs(struct error *err, const char *rootfs, bool no_pivot, bool mount_proc, uid_t uid, gid_t gid, bool *drop_groups)
{
        int rv = -1;
        int oldroot = -1;
        int newroot = -1;
        char buf[8] = {0};
        const char *mounts[] = {"/proc", "/sys", "/dev"};

        error_reset(err);

        /* Create a new mount namespace with private propagation. */
        if (unshare(CLONE_NEWNS) < 0)
                goto fail;
        if (xmount(err, NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) < 0)
                goto fail;
        if (xmount(err, rootfs, rootfs, NULL, MS_BIND|MS_REC, NULL) < 0)
                goto fail;

        /* Pivot to the new rootfs and unmount the previous one. */
        if (no_pivot) {
                if (xmount(err, rootfs, "/", NULL, MS_MOVE, NULL) < 0) {
                        goto fail;
                }
                if ((newroot = xopen(err, rootfs, O_PATH|O_DIRECTORY)) < 0) {
                        log_errf("failed calling xopen %s", rootfs);
                        goto fail;
                }
                if (fchdir(newroot) < 0) {
                        log_errf("failed calling fchdir %d", newroot);
                        goto fail;
                }
        } else {
                if ((oldroot = xopen(err, "/", O_PATH|O_DIRECTORY)) < 0)
                        goto fail;
                if ((newroot = xopen(err, rootfs, O_PATH|O_DIRECTORY)) < 0)
                        goto fail;
                if (fchdir(newroot) < 0)
                        goto fail;
                if (syscall(SYS_pivot_root, ".", ".") < 0)
                        goto fail;
                if (fchdir(oldroot) < 0)
                        goto fail;
                if (umount2(".", MNT_DETACH) < 0)
                        goto fail;
                if (fchdir(newroot) < 0)
                        goto fail;
        }
        if (chroot(".") < 0)
                goto fail;

        if (mount_proc && xmount(err, NULL, "/proc", "proc", MS_RDONLY, NULL) < 0)
                goto fail;

        /*
         * Check if we are in standalone mode, within a user namespace and
         * restricted from setting supplementary groups.
         */
        file_read_line(NULL, PROC_SETGROUPS_PATH(PROC_SELF), buf, sizeof(buf));
        *drop_groups = !str_has_prefix(buf, "deny");

        /* Hide sensitive mountpoints. */
        for (size_t i = mount_proc; i < nitems(mounts); ++i) {
                if (xmount(err, NULL, mounts[i], "tmpfs", MS_RDONLY, NULL) < 0)
                        goto fail;
        }

        /* Temporarily remount /dev with write permissions to create a
         * /dev/fd --> /proc/self/fd symlink. */
        if (xmount(err, NULL, "/dev", "tmpfs", MS_REMOUNT, NULL) < 0)
                goto fail;
        if (file_create(err, "/dev/fd", "/proc/self/fd", uid, gid, MODE_LNK(0777)) < 0)
                goto fail;
        if (xmount(err, NULL, "/dev", "tmpfs", MS_REMOUNT|MS_RDONLY, NULL) < 0)
                goto fail;

        rv = 0;

 fail:
        if (rv < 0 && err->code == 0)
                error_set(err, "process confinement failed");
        xclose(oldroot);
        xclose(newroot);
        return (rv);
}

static int
adjust_capabilities(struct error *err, uid_t uid, bool host_ldconfig)
{
        /*
         * Drop all the inheritable capabilities and the ambient capabilities consequently.
         * Don't bother with the other capabilities, execve will take care of it.
         */
        if (secure_mode() && !host_ldconfig) {
                if (perm_set_capabilities(err, CAP_INHERITABLE, NULL, 0) < 0)
                        return (-1);
                log_warn("running in secure mode without host ldconfig, containers may require additional tuning");
        } else {
                /*
                 * If allowed, set the CAP_DAC_OVERRIDE capability because some distributions rely on it
                 * (e.g. https://bugzilla.redhat.com/show_bug.cgi?id=517575).
                 */
                if (perm_set_capabilities(err, CAP_INHERITABLE, &(cap_value_t){CAP_DAC_OVERRIDE}, 1) < 0) {
                        if (err->code != EPERM)
                                return (-1);
                        if (perm_set_capabilities(err, CAP_INHERITABLE, NULL, 0) < 0)
                                return (-1);
                        log_warn("could not set inheritable capabilities, containers may require additional tuning");
                } else if (uid != 0 && perm_set_capabilities(err, CAP_AMBIENT, &(cap_value_t){CAP_DAC_OVERRIDE}, 1) < 0) {
                        if (err->code != EPERM)
                                return (-1);
                        log_warn("could not set ambient capabilities, containers may require additional tuning");
                }
        }

        /* Drop all the bounding set */
        if (perm_set_bounds(err, NULL, 0) < 0)
                return (-1);

        return (0);
}

static int
adjust_privileges(struct error *err, uid_t uid, gid_t gid, bool drop_groups)
{
        /*
         * Prevent the kernel from adjusting capabilities on UID change.
         * This is necessary if we want to keep our ambient capabilities.
         */
        if (uid != 0 && prctl(PR_SET_SECUREBITS, SECBIT_NO_SETUID_FIXUP, 0, 0, 0) < 0) {
                if (errno == EPERM)
                        log_warn("could not preserve capabilities, containers may require additional tuning");
                else {
                        error_set(err, "privilege change failed");
                        return (-1);
                }
        }
        return (perm_drop_privileges(err, uid, gid, drop_groups));
}

static int
limit_resources(struct error *err)
{
        struct rlimit limit;

        limit = (struct rlimit){10, 10};
        if (setrlimit(RLIMIT_CPU, &limit) < 0)
                goto fail;
        limit = (struct rlimit){2ull*1024*1024*1024, 2ull*1024*1024*1024};
        if (setrlimit(RLIMIT_AS, &limit) < 0)
                goto fail;
        limit = (struct rlimit){64, 64};
        if (setrlimit(RLIMIT_NOFILE, &limit) < 0)
                goto fail;
        limit = (struct rlimit){2*1024*1024, 2*1024*1024};
        if (setrlimit(RLIMIT_FSIZE, &limit) < 0)
                goto fail;
        return (0);

 fail:
        error_set(err, "resource limiting failed");
        return (-1);
}

#ifdef WITH_SECCOMP
static int
limit_syscalls(struct error *err)
{
        scmp_filter_ctx ctx;
        int syscalls[] = {
                SCMP_SYS(access),
                SCMP_SYS(arch_prctl),
                SCMP_SYS(brk),
                SCMP_SYS(chdir),
                SCMP_SYS(chmod),
                SCMP_SYS(close),
                SCMP_SYS(execve),
                SCMP_SYS(execveat),
                SCMP_SYS(exit),
                SCMP_SYS(exit_group),
                SCMP_SYS(fcntl),
                SCMP_SYS(fdatasync),
                SCMP_SYS(fstat),
                SCMP_SYS(fsync),
                SCMP_SYS(ftruncate),
                SCMP_SYS(getcwd),
                SCMP_SYS(getdents),
                SCMP_SYS(getdents64),
                SCMP_SYS(getegid),
                SCMP_SYS(geteuid),
                SCMP_SYS(getgid),
                SCMP_SYS(getpgrp),
                SCMP_SYS(getpid),
                SCMP_SYS(gettid),
                SCMP_SYS(gettimeofday),
                SCMP_SYS(getuid),
                SCMP_SYS(_llseek),
                SCMP_SYS(lseek),
                SCMP_SYS(lstat),
#ifdef SYS_memfd_create
                SCMP_SYS(memfd_create),
#endif
                SCMP_SYS(mkdir),
                SCMP_SYS(mmap),
                SCMP_SYS(mprotect),
                SCMP_SYS(mremap),
                SCMP_SYS(munmap),
                SCMP_SYS(newfstatat),
                SCMP_SYS(open),
                SCMP_SYS(openat),
                SCMP_SYS(pread64),
                SCMP_SYS(read),
                SCMP_SYS(readlink),
                SCMP_SYS(readv),
                SCMP_SYS(rename),
                SCMP_SYS(rt_sigaction),
                SCMP_SYS(rt_sigprocmask),
                SCMP_SYS(rt_sigreturn),
                SCMP_SYS(sendfile),
                SCMP_SYS(stat),
                SCMP_SYS(symlink),
                SCMP_SYS(tgkill),
                SCMP_SYS(time),
                SCMP_SYS(uname),
                SCMP_SYS(unlink),
                SCMP_SYS(write),
                SCMP_SYS(writev),
#if defined(__aarch64__)
                SCMP_SYS(mkdirat),
                SCMP_SYS(unlinkat),
                SCMP_SYS(readlinkat),
                SCMP_SYS(faccessat),
                SCMP_SYS(symlinkat),
                SCMP_SYS(fchmodat),
                SCMP_SYS(renameat),
#endif
        };
        int rv = -1;

        if ((ctx = seccomp_init(SCMP_ACT_ERRNO(EPERM))) == NULL)
                goto fail;
        for (size_t i = 0; i < nitems(syscalls); ++i) {
                if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls[i], 0) < 0)
                        goto fail;
        }
        if (seccomp_load(ctx) < 0)
                goto fail;
        rv = 0;

 fail:
        if (rv < 0)
                error_setx(err, "syscall limiting failed");
        seccomp_release(ctx);
        return (rv);
}
#else
static int
limit_syscalls(struct error *err)
{
        if (secure_mode()) {
                error_setx(err, "running in secure mode with seccomp disabled");
                return (-1);
        }
        log_warn("seccomp is disabled, all syscalls are allowed");
        return (0);
}
#endif /* WITH_SECCOMP */

/* memfd_create(2) flags -- copied from <linux/memfd.h>. */
#ifndef MFD_CLOEXEC
#  define MFD_CLOEXEC       0x0001U
#  define MFD_ALLOW_SEALING 0x0002U
#endif
#ifndef MFD_EXEC
#  define MFD_EXEC          0x0010U
#endif

/* This comes directly from <linux/fcntl.h>. */
#ifndef F_LINUX_SPECIFIC_BASE
#  define F_LINUX_SPECIFIC_BASE 1024
#endif
#ifndef F_ADD_SEALS
#  define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#endif
#ifndef F_SEAL_SEAL
#  define F_SEAL_SEAL          0x0001	/* prevent further seals from being set */
#  define F_SEAL_SHRINK        0x0002	/* prevent file from shrinking */
#  define F_SEAL_GROW          0x0004	/* prevent file from growing */
#  define F_SEAL_WRITE         0x0008	/* prevent writes */
#endif

int memfd_create(const char *name, unsigned int flags)
{
#ifdef SYS_memfd_create
	return syscall(SYS_memfd_create, name, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

static ssize_t
sendfile_nointr(int out_fd, int in_fd, off_t *offset, size_t count)
{
        ssize_t ret;

        do {
                ret = sendfile(out_fd, in_fd, offset, count);
        } while (ret < 0 && errno == EINTR);

        return ret;
}

static int
open_as_memfd(struct error *err, const char *path)
{
        int fd, memfd, ret;
        ssize_t bytes_sent = 0;
        struct stat st = {0};
        off_t offset = 0;

        if ((fd = xopen(err, path, O_RDONLY)) < 0)
                return (-1);

        log_info("creating a virtual copy of the ldconfig binary");
        memfd = memfd_create(path, MFD_ALLOW_SEALING | MFD_CLOEXEC);
        if (memfd == -1) {
                error_set(err, "error creating memfd for path: %s", path);
                return (-1);
        }

        ret = fstat(fd, &st);
        if (ret == -1) {
                error_set(err, "error running fstat for path: %s", path);
                goto fail;
        }

        while (bytes_sent < st.st_size) {
                ssize_t sent;
                sent = sendfile_nointr(memfd, fd, &offset, st.st_size - bytes_sent);
                if (sent == -1) {
                        error_set(err, "failed to copy ldconfig binary to virtual copy");
                        goto fail;
                }
                bytes_sent += sent;
        }

        if (fcntl(memfd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) == -1) {
                error_set(err, "failed to seal virtual copy of the ldconfig binary");
                goto fail;
        }

        close(fd);
        return memfd;
fail:
        close(fd);
        close(memfd);
        return (-1);
}

int
nvc_ldcache_update(struct nvc_context *ctx, const struct nvc_container *cnt)
{
        char **argv;
        pid_t child;
        int status;
        bool drop_groups = true;
        bool host_ldconfig = false;
        int fd = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL) < 0)
                return (-1);

        if (cnt->flags & OPT_CUDA_COMPAT_MODE_LDCONFIG && cnt->cuda_compat_dir != NULL) {
                /*
                 * We include the cuda_compat_dir directory on the ldconfig
                 * command line. This ensures that the CUDA Forward compat
                 * libraries take precendence over the user-mode driver
                 * libraries in the standard library paths (libs_dir and
                 * libs32_dir).
                 * */
                argv = (char * []){cnt->cfg.ldconfig, "-f", "/etc/ld.so.conf", "-C", "/etc/ld.so.cache", cnt->cuda_compat_dir, cnt->cfg.libs_dir, cnt->cfg.libs32_dir, NULL};
        } else {
                argv = (char * []){cnt->cfg.ldconfig, "-f", "/etc/ld.so.conf", "-C", "/etc/ld.so.cache", cnt->cfg.libs_dir, cnt->cfg.libs32_dir, NULL};
        }

        if (*argv[0] == '@') {
                /*
                 * We treat this path specially to be relative to the host filesystem.
                 * Force proc to be remounted since we're creating a PID namespace and fexecve depends on it.
                 */
                ++argv[0];
                if ((fd = open_as_memfd(&ctx->err, argv[0])) < 0) {
                        log_warn("failed to create virtual copy of the ldconfig binary");
                        if ((fd = xopen(&ctx->err, argv[0], O_RDONLY|O_CLOEXEC)) < 0)
                                return (-1);
                }
                host_ldconfig = true;
                log_infof("executing %s from host at %s", argv[0], cnt->cfg.rootfs);
        } else {
                log_infof("executing %s at %s", argv[0], cnt->cfg.rootfs);
        }

        if ((child = create_process(&ctx->err, CLONE_NEWPID|CLONE_NEWIPC)) < 0) {
                xclose(fd);
                return (-1);
        }
        if (child == 0) {
                prctl(PR_SET_NAME, (unsigned long)"nvc:[ldconfig]", 0, 0, 0);

                if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                        goto fail;
                if (adjust_capabilities(&ctx->err, cnt->uid, host_ldconfig) < 0)
                        goto fail;
                if (change_rootfs(&ctx->err, cnt->cfg.rootfs, ctx->no_pivot, host_ldconfig, cnt->uid, cnt->gid, &drop_groups) < 0)
                        goto fail;
                if (limit_resources(&ctx->err) < 0)
                        goto fail;
                if (adjust_privileges(&ctx->err, cnt->uid, cnt->gid, drop_groups) < 0)
                        goto fail;
                if (limit_syscalls(&ctx->err) < 0)
                        goto fail;

                if (fd < 0)
                        execve(argv[0], argv, (char * const []){NULL});
                else
                        fexecve(fd, argv, (char * const []){NULL});
                error_set(&ctx->err, "process execution failed");
         fail:
                log_errf("could not start %s: %s", argv[0], ctx->err.msg);
                (ctx->err.code == ENOENT) ? _exit(EXIT_SUCCESS) : _exit(EXIT_FAILURE);
        }

        xclose(fd);
        if (waitpid(child, &status, 0) < 0) {
                error_set(&ctx->err, "process reaping failed");
                return (-1);
        }
        if (WIFSIGNALED(status)) {
                error_setx(&ctx->err, "process %s terminated with signal %d", argv[0], WTERMSIG(status));
                return (-1);
        }
        if (WIFEXITED(status) && (status = WEXITSTATUS(status)) != 0) {
                error_setx(&ctx->err, "process %s failed with error code: %d", argv[0], status);
                return (-1);
        }
        return (0);
}
