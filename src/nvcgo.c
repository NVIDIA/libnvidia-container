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

#include <sys/types.h>

#include <inttypes.h>

#pragma GCC diagnostic push
#include "nvc_rpc.h"
#pragma GCC diagnostic pop

#include "nvc_internal.h"

#include "error.h"
#include "nvcgo.h"
#include "utils.h"
#include "rpc.h"
#include "xfuncs.h"

void nvcgo_program_1(struct svc_req *, register SVCXPRT *);

static struct nvcgo_ext {
        struct nvcgo;
        void *dl_handle;
} global_nvcgo_context;

int
nvcgo_program_1_freeresult(maybe_unused SVCXPRT *svc, xdrproc_t xdr_result, caddr_t res)
{
        xdr_free(xdr_result, res);
        return (1);
}

struct nvcgo *
nvcgo_get_context(void)
{
        return (struct nvcgo *)&global_nvcgo_context;
}

int
nvcgo_init(struct error *err)
{
        int ret;
        struct nvcgo_ext *ctx = (struct nvcgo_ext *)nvcgo_get_context();
        struct nvcgo_init_res res = {0};

        memset(ctx, 0, sizeof(*ctx));
        if (rpc_init(err, &ctx->rpc, NVCGO_PROGRAM, NVCGO_VERSION, nvcgo_program_1) < 0)
                goto fail;

        ret = call_rpc(err, &ctx->rpc, &res, nvcgo_init_1);
        xdr_free((xdrproc_t)xdr_nvcgo_init_res, (caddr_t)&res);
        if (ret < 0)
                goto fail;

        return (0);

 fail:
        rpc_shutdown(NULL, &ctx->rpc, true);
        return (-1);
}

bool_t
nvcgo_init_1_svc(ptr_t ctxptr, nvcgo_init_res *res, maybe_unused struct svc_req *req)
{
        struct error *err = (struct error[]){0};
        struct nvcgo_ext *ctx = (struct nvcgo_ext *)ctxptr;
        void *handle;

        memset(res, 0, sizeof(*res));

        #define funcs_entry(name) \
            {(const void**)&ctx->api.name, #name}

        struct {
                const void **f;
                const char *symbol;
        } funcs[] = {
                funcs_entry(GetDeviceCGroupVersion),
                funcs_entry(GetDeviceCGroupMountPath),
                funcs_entry(GetDeviceCGroupRootPath),
                funcs_entry(AddDeviceRules),
        };

        const size_t len_libnvcgo = sizeof(struct libnvcgo)/sizeof(void*);
        const size_t len_funcs = sizeof(funcs)/sizeof(*funcs);
        static_assert(len_funcs == len_libnvcgo, "len(libnvcgo) != len(funcs)");

        if ((handle = xdlopen(err, SONAME_LIBNVCGO, RTLD_NOW)) == NULL)
                goto fail;

        for (size_t i = 0; i < len_funcs; i++) {
                *funcs[i].f = dlsym(handle, funcs[i].symbol);
                if (!*funcs[i].f) {
                        dlclose(handle);
                        error_setx(err, "dlsym error: %s", dlerror());
                        goto fail;
                }
        }

        ctx->dl_handle = handle;
        return (true);

 fail:
        error_to_xdr(err, res);
        return (true);
}

int
nvcgo_shutdown(struct error *err)
{
        int ret;
        struct nvcgo_ext *ctx = (struct nvcgo_ext *)nvcgo_get_context();
        struct nvcgo_shutdown_res res = {0};

        ret = call_rpc(err, &ctx->rpc, &res, nvcgo_shutdown_1);
        xdr_free((xdrproc_t)xdr_nvcgo_shutdown_res, (caddr_t)&res);
        if (rpc_shutdown(err, &ctx->rpc, (ret < 0)) < 0)
                return (-1);

        return (0);
}

bool_t
nvcgo_shutdown_1_svc(ptr_t ctxptr, nvcgo_shutdown_res *res, maybe_unused struct svc_req *req)
{
        struct nvcgo_ext *ctx = (struct nvcgo_ext *)ctxptr;
        memset(res, 0, sizeof(*res));
        xdlclose(NULL, ctx->dl_handle);
        svc_exit();
        return (true);
}
