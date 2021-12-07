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

static int
setup_client(struct error *err, struct rpc *rpc)
{
        struct sockaddr_un addr;
        socklen_t addrlen;
        struct timeval timeout = {10, 0};

        xclose(rpc->fd[SOCK_SVC]);

        addrlen = sizeof(addr);
        if (getpeername(rpc->fd[SOCK_CLT], (struct sockaddr *)&addr, &addrlen) < 0) {
                error_set(err, "%s rpc address resolution failed", rpc->prog.name);
                return (-1);
        }
        if ((rpc->clt = clntunix_create(&addr, rpc->prog.id, rpc->prog.version, &rpc->fd[SOCK_CLT], 0, 0)) == NULL) {
                error_setx(err, "%s rpc %s", rpc->prog.name, clnt_spcreateerror("client creation failed"));
                return (-1);
        }
        clnt_control(rpc->clt, CLSET_TIMEOUT, (char *)&timeout);
        return (0);
}

static noreturn void
setup_service(struct error *err, struct rpc *rpc, pid_t ppid)
{
        char procname[16];
        int rv = EXIT_FAILURE;

        log_infof("starting %s rpc service", rpc->prog.name);
        snprintf(procname, 16, "nvc:[%s]", rpc->prog.name);
        prctl(PR_SET_NAME, (unsigned long)procname, 0, 0, 0);

        xclose(rpc->fd[SOCK_CLT]);

        /*
         * Set PDEATHSIG in case our parent terminates unexpectedly.
         * We need to do it late since the kernel resets it on credential change.
         */
        if (prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0) < 0) {
                error_set(err, "%s rpc service initialization failed", rpc->prog.name);
                goto fail;
        }
        if (getppid() != ppid)
                kill(getpid(), SIGTERM);

        if ((rpc->svc = svcunixfd_create(rpc->fd[SOCK_SVC], 0, 0)) == NULL ||
            !svc_register(rpc->svc, rpc->prog.id, rpc->prog.version, rpc->prog.dispatch, 0)) {
                error_setx(err, "%s rpc service registration failed", rpc->prog.name);
                goto fail;
        }
        svc_run();

        log_infof("terminating %s rpc service", rpc->prog.name);
        rv = EXIT_SUCCESS;

 fail:
        if (rv != EXIT_SUCCESS)
                log_errf("could not start %s rpc service: %s", rpc->prog.name, err->msg);
        if (rpc->svc != NULL)
                svc_destroy(rpc->svc);
        _exit(rv);
}

static int
reap_process(struct error *err, struct rpc *rpc, int fd, bool force)
{
        int ret = 0;
        int status;
        struct pollfd fds = {.fd = fd, .events = POLLRDHUP};

        if (waitpid(rpc->pid, &status, WNOHANG) <= 0) {
                if (force)
                        kill(rpc->pid, SIGTERM);

                switch ((ret = poll(&fds, 1, REAP_TIMEOUT_MS))) {
                case -1:
                        break;
                case 0:
                        log_warnf("terminating %s rpc service (forced)", rpc->prog.name);
                        ret = kill(rpc->pid, SIGKILL);
                        /* Fallthrough */
                default:
                        if (ret >= 0)
                                ret = waitpid(rpc->pid, &status, 0);
                }
        }
        if (ret < 0)
                error_set(err, "reaping %s rpc service process failed (pid %"PRId32")", rpc->prog.name, (int32_t)rpc->pid);
        else
                log_infof("%s rpc service terminated %s%.0d",
                    rpc->prog.name,
                    WIFSIGNALED(status) ? "with signal " : "successfully",
                    WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        return (ret);
}

int
rpc_init(struct error *err, struct rpc *rpc, struct rpc_prog *prog)
{
        pid_t pid;

        if (rpc->initialized)
                return (0);

        *rpc = (struct rpc){false, {-1, -1}, -1, NULL, NULL, *prog};

        pid = getpid();
        if (socketpair(PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, rpc->fd) < 0 || (rpc->pid = fork()) < 0) {
                error_set(err, "%s rpc service process creation failed", rpc->prog.name);
                goto fail;
        }
        if (rpc->pid == 0)
                setup_service(err, rpc, pid);
        if (setup_client(err, rpc) < 0)
                goto fail;

        rpc->initialized = true;
        return (0);

 fail:
        if (rpc->pid > 0 && reap_process(NULL, rpc, rpc->fd[SOCK_CLT], true) < 0)
                log_warnf("could not terminate %s rpc service (pid %"PRId32")", rpc->prog.name, (int32_t)rpc->pid);
        if (rpc->clt != NULL)
                clnt_destroy(rpc->clt);

        xclose(rpc->fd[SOCK_CLT]);
        xclose(rpc->fd[SOCK_SVC]);
        return (-1);
}

int
rpc_shutdown(struct error *err, struct rpc *rpc, bool force)
{
        if (rpc->pid > 0 && reap_process(err, rpc, rpc->fd[SOCK_CLT], force) < 0) {
                log_warnf("could not terminate %s rpc service: %s", rpc->prog.name, err->msg);
                return (-1);
        }
        if (rpc->clt != NULL)
                clnt_destroy(rpc->clt);

        xclose(rpc->fd[SOCK_CLT]);
        xclose(rpc->fd[SOCK_SVC]);
        *rpc = (struct rpc){false, {-1, -1}, -1, NULL, NULL, {0}};
        return (0);
}
