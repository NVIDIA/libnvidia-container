/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#pragma GCC diagnostic push
#include "nvc_rpc.h"
#pragma GCC diagnostic pop

#include "error.h"
#include "utils.h"
#include "rpc.h"
#include "xfuncs.h"

#define REAP_TIMEOUT_MS 10

int
rpc_setup_client(struct rpc *ctx)
{
        struct sockaddr_un addr;
        socklen_t addrlen;
        struct timeval timeout = {10, 0};

        xclose(ctx->fd[SOCK_SVC]);

        addrlen = sizeof(addr);
        if (getpeername(ctx->fd[SOCK_CLT], (struct sockaddr *)&addr, &addrlen) < 0) {
                error_set(ctx->err, "address resolution failed");
                return (-1);
        }
        if ((ctx->rpc_clt = clntunix_create(&addr, ctx->rpc_prognum, ctx->rpc_versnum, &ctx->fd[SOCK_CLT], 0, 0)) == NULL) {
                error_setx(ctx->err, "%s", clnt_spcreateerror("rpc client creation failed"));
                return (-1);
        }
        clnt_control(ctx->rpc_clt, CLSET_TIMEOUT, (char *)&timeout);
        return (0);
}

noreturn void
rpc_setup_service(struct rpc *ctx, const char *root, uid_t uid, gid_t gid, pid_t ppid)
{
        int rv = EXIT_FAILURE;

        log_info("starting rpc service");
        prctl(PR_SET_NAME, (unsigned long)"nvc:[rpc]", 0, 0, 0);

        xclose(ctx->fd[SOCK_CLT]);

        if (!str_equal(root, "/")) {
                /* Preload glibc libraries to avoid symbols mismatch after changing root. */
                if (xdlopen(ctx->err, "libm.so.6", RTLD_NOW) == NULL)
                        goto fail;
                if (xdlopen(ctx->err, "librt.so.1", RTLD_NOW) == NULL)
                        goto fail;
                if (xdlopen(ctx->err, "libpthread.so.0", RTLD_NOW) == NULL)
                        goto fail;

                if (chroot(root) < 0 || chdir("/") < 0) {
                        error_set(ctx->err, "change root failed");
                        goto fail;
                }
        }

        /*
         * Drop privileges and capabilities for security reasons.
         *
         * We might be inside a user namespace with full capabilities, this should also help prevent NVML
         * from potentially adjusting the host device nodes based on the (wrong) driver registry parameters.
         *
         * If we are not changing group, then keep our supplementary groups as well.
         * This is arguable but allows us to support unprivileged processes (i.e. without CAP_SETGID) and user namespaces.
         */
        if (perm_drop_privileges(ctx->err, uid, gid, (getegid() != gid)) < 0)
                goto fail;
        if (perm_set_capabilities(ctx->err, CAP_PERMITTED, NULL, 0) < 0)
                goto fail;

        /*
         * Set PDEATHSIG in case our parent terminates unexpectedly.
         * We need to do it late since the kernel resets it on credential change.
         */
        if (prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0) < 0) {
                error_set(ctx->err, "process initialization failed");
                goto fail;
        }
        if (getppid() != ppid)
                kill(getpid(), SIGTERM);


        if ((ctx->rpc_svc = svcunixfd_create(ctx->fd[SOCK_SVC], 0, 0)) == NULL ||
            !svc_register(ctx->rpc_svc, ctx->rpc_prognum, ctx->rpc_versnum, ctx->rpc_dispatch, 0)) {
                error_setx(ctx->err, "program registration failed");
                goto fail;
        }
        svc_run();

        log_info("terminating rpc service");
        rv = EXIT_SUCCESS;

 fail:
        if (rv != EXIT_SUCCESS)
                log_errf("could not start rpc service: %s", ctx->err->msg);
        if (ctx->rpc_svc != NULL)
                svc_destroy(ctx->rpc_svc);
        _exit(rv);
}

int
rpc_reap_process(struct error *err, pid_t pid, int fd, bool force)
{
        int ret = 0;
        int status;
        struct pollfd fds = {.fd = fd, .events = POLLRDHUP};

        if (waitpid(pid, &status, WNOHANG) <= 0) {
                if (force)
                        kill(pid, SIGTERM);

                switch ((ret = poll(&fds, 1, REAP_TIMEOUT_MS))) {
                case -1:
                        break;
                case 0:
                        log_warn("terminating rpc service (forced)");
                        ret = kill(pid, SIGKILL);
                        /* Fallthrough */
                default:
                        if (ret >= 0)
                                ret = waitpid(pid, &status, 0);
                }
        }
        if (ret < 0)
                error_set(err, "process reaping failed (pid %"PRId32")", (int32_t)pid);
        else
                log_infof("rpc service terminated %s%.0d",
                    WIFSIGNALED(status) ? "with signal " : "successfully",
                    WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        return (ret);
}
