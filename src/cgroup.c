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

#include <sys/sysmacros.h>

#pragma GCC diagnostic push
#include "nvc_rpc.h"
#pragma GCC diagnostic pop

#include "cgroup.h"
#include "options.h"
#include "nvcgo.h"
#include "rpc.h"

int
get_device_cgroup_version(struct error *err, const struct nvc_container *cnt)
{
        struct nvcgo_get_device_cgroup_version_res res = {0};
        struct nvcgo *nvcgo = nvcgo_get_context();
        int rv = -1;

        const char* proc_root = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "/";

        if (call_rpc(err, &nvcgo->rpc, &res, nvcgo_get_device_cgroup_version_1, (char*)proc_root, cnt->cfg.pid) < 0)
                goto fail;
        rv = (int)res.nvcgo_get_device_cgroup_version_res_u.vers;

 fail:
        xdr_free((xdrproc_t)xdr_nvcgo_get_device_cgroup_version_res, (caddr_t)&res);
        return (rv);
}

bool_t
nvcgo_get_device_cgroup_version_1_svc(ptr_t ctxptr, char *proc_root, pid_t pid, nvcgo_get_device_cgroup_version_res *res, maybe_unused struct svc_req *req)
{
        struct error *err = (struct error[]){0};
        struct nvcgo *nvcgo = (struct nvcgo *)ctxptr;
        int version = -1;
        char *rerr = NULL;
        int rv = -1;

        memset(res, 0, sizeof(*res));

        // Explicitly set CAP_EFFECTIVE to NVC_CONTAINER across the
        // 'GetDeviceCGroupVersion()' call.  This is only done because we
        // happen to know these are the effective capabilities set by the
        // nvidia-container-cli (i.e. the only known user of this library)
        // anytime this RPC handler is invoked. In the future we should
        // consider setting effective capabilities on the server to match
        // whatever capabilities were in effect in the client when the RPC call
        // was made.
        if (perm_set_capabilities(err, CAP_EFFECTIVE, ecaps[NVC_CONTAINER], ecaps_size(NVC_CONTAINER)) < 0)
                goto fail;

        if ((rv = nvcgo->api.GetDeviceCGroupVersion(proc_root, pid, &version, &rerr) < 0)) {
                error_setx(err, "failed to get device cgroup version: %s", rerr);
                goto fail;
        }

        res->nvcgo_get_device_cgroup_version_res_u.vers = (unsigned int)version;
        rv = 0;

 fail:
        free(rerr);
        if (perm_set_capabilities(err, CAP_EFFECTIVE, NULL, 0) < 0)
                rv = -1;
        if (rv < 0)
                error_to_xdr(err, res);
        return (true);
}

char *
find_device_cgroup_path(struct error *err, const struct nvc_container *cnt)
{
        struct nvcgo_find_device_cgroup_path_res res = {0};
        struct nvcgo *nvcgo = nvcgo_get_context();
        char *cgroup_path = NULL;

        pid_t pid = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.pid : getppid();
        const char* proc_root = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "/";

        if (call_rpc(err, &nvcgo->rpc, &res, nvcgo_find_device_cgroup_path_1, cnt->dev_cg_version, (char*)proc_root, pid, cnt->cfg.pid) < 0)
                goto fail;
        if ((cgroup_path = xstrdup(err, res.nvcgo_find_device_cgroup_path_res_u.cgroup_path)) == NULL)
                goto fail;

 fail:
        xdr_free((xdrproc_t)xdr_nvcgo_find_device_cgroup_path_res, (caddr_t)&res);
        return (cgroup_path);
}

bool_t
nvcgo_find_device_cgroup_path_1_svc(ptr_t ctxptr, int dev_cg_version, char *proc_root, pid_t mp_pid, pid_t rp_pid, nvcgo_find_device_cgroup_path_res *res, maybe_unused struct svc_req *req)
{
        struct error *err = (struct error[]){0};
        struct nvcgo *nvcgo = (struct nvcgo *)ctxptr;
        char path[PATH_MAX] = {};
        char *cgroup_mount_prefix = NULL;
        char *cgroup_mount = NULL;
        char *cgroup_root = NULL;
        char *cgroup_path = NULL;
        char *rerr = NULL;
        int rv = -1;

        memset(res, 0, sizeof(*res));

        // Explicitly set CAP_EFFECTIVE to NVC_CONTAINER across the
        // 'GetDeviceCGroupMountPath()' and 'GetDeviceCGroupRootPath()' calls.
        // This is only done because we happen to know these are the effective
        // capabilities set by the nvidia-container-cli (i.e. the only known
        // user of this library) anytime this RPC handler is invoked. In the
        // future we should consider setting effective capabilities on the
        // server to match whatever capabilities were in effect in the client
        // when the RPC call was made.
        if (perm_set_capabilities(err, CAP_EFFECTIVE, ecaps[NVC_CONTAINER], ecaps_size(NVC_CONTAINER)) < 0)
                goto fail;

        if ((rv = nvcgo->api.GetDeviceCGroupMountPath(dev_cg_version, proc_root, mp_pid, &cgroup_mount_prefix, &cgroup_mount, &rerr)) < 0) {
                error_setx(err, "failed to get device cgroup mount path: %s", rerr);
                goto fail;
        }

        if ((rv = nvcgo->api.GetDeviceCGroupRootPath(dev_cg_version, proc_root, cgroup_mount_prefix, rp_pid, &cgroup_root, &rerr)) < 0) {
                error_setx(err, "failed to get device cgroup root path: %s", rerr);
                goto fail;
        }

        if ((rv = path_new(err, path, proc_root)) < 0)
                goto fail;
        if ((rv = path_append(err, path, cgroup_mount)) < 0)
                goto fail;
        if ((rv = path_append(err, path, cgroup_root)) < 0)
                goto fail;

        if ((cgroup_path = xstrdup(err, path)) == NULL)
                goto fail;

        res->nvcgo_find_device_cgroup_path_res_u.cgroup_path = cgroup_path;
        rv = 0;

 fail:
        free(rerr);
        free(cgroup_mount);
        free(cgroup_root);
        if (perm_set_capabilities(err, CAP_EFFECTIVE, NULL, 0) < 0)
                rv = -1;
        if (rv < 0)
                error_to_xdr(err, res);
        return (true);
}

int
setup_device_cgroup(struct error *err, const struct nvc_container *cnt, dev_t id)
{
        struct nvcgo_setup_device_cgroup_res res = {0};
        struct nvcgo *nvcgo = nvcgo_get_context();
        int rv = -1;

        if (call_rpc(err, &nvcgo->rpc, &res, nvcgo_setup_device_cgroup_1, cnt->dev_cg_version, cnt->dev_cg, id) < 0)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_nvcgo_find_device_cgroup_path_res, (caddr_t)&res);
        return (rv);
}

bool_t
nvcgo_setup_device_cgroup_1_svc(ptr_t ctxptr, int dev_cg_version, char *dev_cg, dev_t id, nvcgo_setup_device_cgroup_res *res, maybe_unused struct svc_req *req)
{
        struct error *err = (struct error[]){0};
        struct nvcgo *nvcgo = (struct nvcgo *)ctxptr;
        char *rerr = NULL;
        int rv = -1;

        struct device_rule rules[] = {
                {
                        .allow  = true,
                        .type   = "c",
                        .access = "rw",
                        .major  = major(id),
                        .minor  = minor(id),
                },
        };

        GoSlice rules_slice = {
                .data = &rules,
                .len = sizeof(rules)/sizeof(rules[0]),
                .cap = sizeof(rules)/sizeof(rules[0]),
        };

        memset(res, 0, sizeof(*res));

        // Explicitly set CAP_EFFECTIVE to NVC_MOUNT across the 'AddDeviceRules()' call.
        // This is only done because we happen to know these are the effective
        // capabilities set by the nvidia-container-cli (i.e. the only known
        // user of this library) anytime this RPC handler is invoked. In the
        // future we should consider setting effective capabilities on the
        // server to match whatever capabilities were in effect in the client
        // when the RPC call was made.
        if (perm_set_capabilities(err, CAP_EFFECTIVE, ecaps[NVC_MOUNT], ecaps_size(NVC_MOUNT)) < 0)
                goto fail;

        if ((rv = nvcgo->api.AddDeviceRules(dev_cg_version, dev_cg, rules_slice, &rerr)) < 0) {
                error_setx(err, "failed to add device rules: %s", rerr);
                goto fail;
        }

        rv = 0;

fail:
        free(rerr);
        if (perm_set_capabilities(err, CAP_EFFECTIVE, NULL, 0) < 0)
                rv = -1;
        if (rv < 0)
                error_to_xdr(err, res);
        return (true);
}
