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

#ifndef HEADER_RPC_H
#define HEADER_RPC_H

#include <sys/types.h>

#include <rpc/rpc.h>

#ifndef WITH_TIRPC
/* Glibc is missing this prototype */
SVCXPRT *svcunixfd_create(int, u_int, u_int);
#endif /* WITH_TIRPC */

#include <stdbool.h>

#include "error.h"
#include "dxcore.h"

#define SOCK_CLT 0
#define SOCK_SVC 1

struct rpc {
        bool initialized;
        int fd[2];
        pid_t pid;
        SVCXPRT *svc;
        CLIENT *clt;
        unsigned long prognum;
        unsigned long versnum;
        void (*dispatch)(struct svc_req *, SVCXPRT *);
};

int rpc_init(struct error *, struct rpc *, const char *, uid_t, gid_t, unsigned long, unsigned long, void (*dispatch)(struct svc_req *, SVCXPRT *));
int rpc_shutdown(struct error *, struct rpc *, bool force);

#define call_rpc(err, ctx, res, func, ...) __extension__ ({                                            \
        enum clnt_stat r_;                                                                             \
        struct sigaction osa_, sa_ = {.sa_handler = SIG_IGN};                                          \
                                                                                                       \
        static_assert(sizeof(ptr_t) >= sizeof(intptr_t), "incompatible types");                        \
        sigaction(SIGPIPE, &sa_, &osa_);                                                               \
        if ((r_ = func((ptr_t)ctx, ##__VA_ARGS__, res, (ctx)->clt)) != RPC_SUCCESS)                    \
                error_set_rpc(err, r_, "rpc error");                                                   \
        else if ((res)->errcode != 0)                                                                  \
                error_from_xdr(err, res);                                                              \
        sigaction(SIGPIPE, &osa_, NULL);                                                               \
        (r_ == RPC_SUCCESS && (res)->errcode == 0) ? 0 : -1;                                           \
})

#endif /* HEADER_RPC_H */
