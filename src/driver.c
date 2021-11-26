/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <inttypes.h>

#include "nvml.h"

#pragma GCC diagnostic push
#include "driver_rpc.h"
#pragma GCC diagnostic pop

#include "nvc_internal.h"

#include "driver.h"
#include "error.h"
#include "utils.h"
#include "rpc.h"
#include "xfuncs.h"

#define MAX_DEVICES     64
#define MAX_MIG_DEVICES  8

void driver_program_1(struct svc_req *, register SVCXPRT *);

struct mig_device {
        nvmlDevice_t nvml;
};

static struct driver_device {
        nvmlDevice_t nvml;
        struct mig_device mig[MAX_MIG_DEVICES];
} device_handles[MAX_DEVICES];

#define call_nvml(ctx, sym, ...) __extension__ ({                                                      \
        union {void *ptr; __typeof__(&sym) fn;} u_;                                                    \
        nvmlReturn_t r_;                                                                               \
                                                                                                       \
        dlerror();                                                                                     \
        u_.ptr = dlsym((ctx)->nvml_dl, #sym);                                                          \
        r_ = (dlerror() == NULL) ? (*u_.fn)(__VA_ARGS__) : NVML_ERROR_FUNCTION_NOT_FOUND;              \
        if (r_ != NVML_SUCCESS)                                                                        \
                error_set_nvml((ctx)->err, (ctx)->nvml_dl, r_, "nvml error");                          \
        (r_ == NVML_SUCCESS) ? 0 : -1;                                                                 \
})

int
driver_program_1_freeresult(maybe_unused SVCXPRT *svc, xdrproc_t xdr_result, caddr_t res)
{
        xdr_free(xdr_result, res);
        return (1);
}

int
driver_init(struct driver *ctx, struct error *err, struct dxcore_context *dxcore, const char *root, uid_t uid, gid_t gid)
{
        int ret;
        pid_t pid;
        char nvml_path[PATH_MAX] = SONAME_LIBNVML;
        struct driver_init_res res = {0};

        *ctx = (struct driver){{err, NULL, {-1, -1}, -1, NULL, NULL, DRIVER_PROGRAM, DRIVER_VERSION, driver_program_1}};

        pid = getpid();
        if (socketpair(PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, ctx->fd) < 0 || (ctx->pid = fork()) < 0) {
                error_set(err, "process creation failed");
                goto fail;
        }
        if (ctx->pid == 0)
                rpc_setup_service(ctx, root, uid, gid, pid);
        if (rpc_setup_client(ctx) < 0)
                goto fail;

        if (dxcore->initialized) {
                memset(nvml_path, 0, strlen(nvml_path));
                if (path_join(err, nvml_path, dxcore->adapterList[0].pDriverStorePath, SONAME_LIBNVML) < 0)
                        goto fail;
        }

        ret = call_rpc(ctx, &res, driver_init_1, nvml_path);
        xdr_free((xdrproc_t)xdr_driver_init_res, (caddr_t)&res);
        if (ret < 0)
                goto fail;

        return (0);

 fail:
        if (ctx->pid > 0 && rpc_reap_process(NULL, ctx->pid, ctx->fd[SOCK_CLT], true) < 0)
                log_warnf("could not terminate rpc service (pid %"PRId32")", (int32_t)ctx->pid);
        if (ctx->rpc_clt != NULL)
                clnt_destroy(ctx->rpc_clt);

        xclose(ctx->fd[SOCK_CLT]);
        xclose(ctx->fd[SOCK_SVC]);
        return (-1);
}

bool_t
driver_init_1_svc(ptr_t ctxptr, char *nvml_path, driver_init_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;

        memset(res, 0, sizeof(*res));

        if ((ctx->nvml_dl = xdlopen(ctx->err, nvml_path, RTLD_NOW)) == NULL)
                goto fail;

        if (call_nvml(ctx, nvmlInit_v2) < 0)
                goto fail;

        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_shutdown(struct driver *ctx)
{
        int ret;
        struct driver_shutdown_res res = {0};

        ret = call_rpc(ctx, &res, driver_shutdown_1);
        xdr_free((xdrproc_t)xdr_driver_shutdown_res, (caddr_t)&res);
        if (ret < 0)
                log_warnf("could not terminate rpc service: %s", ctx->err->msg);

        if (rpc_reap_process(ctx->err, ctx->pid, ctx->fd[SOCK_CLT], (ret < 0)) < 0)
                return (-1);
        clnt_destroy(ctx->rpc_clt);

        xclose(ctx->fd[SOCK_CLT]);
        xclose(ctx->fd[SOCK_SVC]);
        *ctx = (struct driver){{NULL, NULL, {-1, -1}, -1, NULL, NULL, 0, 0, NULL}};
        return (0);
}

bool_t
driver_shutdown_1_svc(ptr_t ctxptr, driver_shutdown_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        int rv = -1;

        memset(res, 0, sizeof(*res));
        if ((rv = call_nvml(ctx, nvmlShutdown)) < 0)
                goto fail;
        svc_exit();

 fail:
        if (rv < 0)
                error_to_xdr(ctx->err, res);
        xdlclose(NULL, ctx->nvml_dl);
        return (true);
}

int
driver_get_rm_version(struct driver *ctx, char **version)
{
        struct driver_get_rm_version_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_rm_version_1) < 0)
                goto fail;
        if ((*version = xstrdup(ctx->err, res.driver_get_rm_version_res_u.vers)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_rm_version_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_rm_version_1_svc(ptr_t ctxptr, driver_get_rm_version_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        char buf[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlSystemGetDriverVersion, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_rm_version_res_u.vers = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_cuda_version(struct driver *ctx, char **version)
{
        struct driver_get_cuda_version_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_cuda_version_1) < 0)
                goto fail;
        if (xasprintf(ctx->err, version, "%u.%u", res.driver_get_cuda_version_res_u.vers.major,
            res.driver_get_cuda_version_res_u.vers.minor) < 0)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_cuda_version_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_cuda_version_1_svc(ptr_t ctxptr, driver_get_cuda_version_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        int version;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlSystemGetCudaDriverVersion, &version) < 0)
                goto fail;

        res->driver_get_cuda_version_res_u.vers.major = (unsigned int)version / 1000;
        res->driver_get_cuda_version_res_u.vers.minor = (unsigned int)version % 100 / 10;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_count(struct driver *ctx, unsigned int *count)
{
        struct driver_get_device_count_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_count_1) < 0)
                goto fail;
        *count = res.driver_get_device_count_res_u.count;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_count_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_count_1_svc(ptr_t ctxptr, driver_get_device_count_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        unsigned int count;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetCount_v2, &count) < 0)
                goto fail;
        res->driver_get_device_count_res_u.count = count;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device(struct driver *ctx, unsigned int idx, struct driver_device **dev)
{
        struct driver_get_device_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_1, idx) < 0)
                goto fail;
        *dev = (struct driver_device *)res.driver_get_device_res_u.dev;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_1_svc(ptr_t ctxptr, u_int idx, driver_get_device_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;

        memset(res, 0, sizeof(*res));
        if (idx >= MAX_DEVICES) {
                error_setx(ctx->err, "too many devices");
                goto fail;
        }
        if (call_nvml(ctx, nvmlDeviceGetHandleByIndex_v2, (unsigned)idx, &device_handles[idx].nvml) < 0)
                goto fail;

        res->driver_get_device_res_u.dev = (ptr_t)&device_handles[idx];
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_minor(struct driver *ctx, struct driver_device *dev, unsigned int *minor)
{
        struct driver_get_device_minor_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_minor_1, (ptr_t)dev) < 0)
                goto fail;
        *minor = res.driver_get_device_minor_res_u.minor;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_minor_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_minor_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_minor_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        unsigned int minor;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetMinorNumber, handle->nvml, &minor) < 0)
                goto fail;
        res->driver_get_device_minor_res_u.minor = minor;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_busid(struct driver *ctx, struct driver_device *dev, char **busid)
{
        struct driver_get_device_busid_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_busid_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*busid = xstrdup(ctx->err, res.driver_get_device_busid_res_u.busid)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_busid_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_busid_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_busid_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        nvmlPciInfo_t pci;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetPciInfo, handle->nvml, &pci) < 0)
                goto fail;

        if (xasprintf(ctx->err, &res->driver_get_device_busid_res_u.busid, "%08x:%02x:%02x.0", pci.domain, pci.bus, pci.device) < 0)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_uuid(struct driver *ctx, struct driver_device *dev, char **uuid)
{
        struct driver_get_device_uuid_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_uuid_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*uuid = xstrdup(ctx->err, res.driver_get_device_uuid_res_u.uuid)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_uuid_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_uuid_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_uuid_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        char buf[NVML_DEVICE_UUID_V2_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetUUID, handle->nvml, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_device_uuid_res_u.uuid = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_model(struct driver *ctx, struct driver_device *dev, char **model)
{
        struct driver_get_device_model_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_model_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*model = xstrdup(ctx->err, res.driver_get_device_model_res_u.model)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_model_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_model_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_model_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        char buf[NVML_DEVICE_NAME_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetName, handle->nvml, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_device_model_res_u.model = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_brand(struct driver *ctx, struct driver_device *dev, char **brand)
{
        struct driver_get_device_brand_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_brand_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*brand = xstrdup(ctx->err, res.driver_get_device_brand_res_u.brand)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_brand_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_brand_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_brand_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        nvmlBrandType_t brand;
        const char *buf;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetBrand, handle->nvml, &brand) < 0)
                goto fail;
        switch (brand) {
        case NVML_BRAND_QUADRO:
                buf = "Quadro";
                break;
        case NVML_BRAND_TESLA:
                buf = "Tesla";
                break;
        case NVML_BRAND_NVS:
                buf = "NVS";
                break;
        case NVML_BRAND_GRID:
                buf = "GRID";
                break;
        case NVML_BRAND_GEFORCE:
                buf = "GeForce";
                break;
        case NVML_BRAND_TITAN:
                buf = "TITAN";
                break;
        default:
                buf = "Unknown";
        }
        if ((res->driver_get_device_brand_res_u.brand = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_arch(struct driver *ctx, struct driver_device *dev, char **arch)
{
        struct driver_get_device_arch_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_arch_1, (ptr_t)dev) < 0)
                goto fail;
        if (xasprintf(ctx->err, arch, "%u.%u", res.driver_get_device_arch_res_u.arch.major,
            res.driver_get_device_arch_res_u.arch.minor) < 0)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_arch_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_arch_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_arch_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        int major, minor;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetCudaComputeCapability, handle->nvml, &major, &minor) < 0)
                goto fail;

        res->driver_get_device_arch_res_u.arch.major = (unsigned int)major;
        res->driver_get_device_arch_res_u.arch.minor = (unsigned int)minor;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_mig_enabled(struct driver *ctx, struct driver_device *dev, bool *enabled)
{
        // Initialize local variables.
        unsigned int current, pending;
        struct driver_get_device_mig_mode_res res = {0};
        int rv = -1;

        // Initialize return values.
        *enabled = false;

        // Make an RPC call to determine if MIG mode is enabled or not.
        if (call_rpc(ctx, &res, driver_get_device_mig_mode_1, (ptr_t)dev) < 0)
                goto fail;

        switch(res.driver_get_device_mig_mode_res_u.mode.error) {
                // to determine if MIG mode is enabled or not.
                case NVML_SUCCESS:
                        current = res.driver_get_device_mig_mode_res_u.mode.current;
                        pending = res.driver_get_device_mig_mode_res_u.mode.pending;
                        *enabled = (current == NVML_DEVICE_MIG_ENABLE) && (current == pending);
                        break;
                // If the error indicates that the function wasn't found, then
                // we are on an older version of NVML that doesn't support MIG.
                // That's OK, we just need to set enabled to false in this
                // case. We do the same if we determine that MIG is not
                // supported on this device.
                case NVML_ERROR_FUNCTION_NOT_FOUND:
                case NVML_ERROR_NOT_SUPPORTED:
                        *enabled = false;
                        break;
                // In all other cases, fail this function.
                default:
                        goto fail;
        }

        // Set 'rv' to 0 to indicate success.
        rv = 0;
 fail:
        // Free the results of the RPC call and return.
        xdr_free((xdrproc_t)xdr_driver_get_device_arch_res, (caddr_t)&res);
        return (rv);
}

int
driver_get_device_mig_capable(struct driver *ctx, struct driver_device *dev, bool *supported)
{
        // Initialize local variables.
        struct driver_get_device_mig_mode_res res = {0};
        int rv = -1;

        // Initialize return values.
        *supported= false;

        // Make an RPC call to determine if MIG mode is enabled or not.
        if (call_rpc(ctx, &res, driver_get_device_mig_mode_1, (ptr_t)dev) < 0)
                goto fail;

        switch(res.driver_get_device_mig_mode_res_u.mode.error) {
                case NVML_SUCCESS:
                        *supported = true;
                        break;
                case NVML_ERROR_FUNCTION_NOT_FOUND:
                case NVML_ERROR_NOT_SUPPORTED:
                        *supported = false;
                        break;
                // In all other cases, fail this function.
                default:
                        goto fail;
        }

        // Set 'rv' to 0 to indicate success.
        rv = 0;
 fail:
        // Free the results of the RPC call and return.
        xdr_free((xdrproc_t)xdr_driver_get_device_arch_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_mig_mode_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_mig_mode_res *res, maybe_unused struct svc_req *req)
{
        // Initialize local variables.
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        unsigned int current, pending;

        // Clear out 'res' which will hold the result of this RPC call.
        memset(res, 0, sizeof(*res));

        // Call into NVML to get the MIG mode. We don't directly catch the
        // error here and return a failure. Instead, we capture the error and
        // pass it as part of the return value for the caller to interpret.
        if(call_nvml(ctx, nvmlDeviceGetMigMode, handle->nvml, &current, &pending) < 0)
                res->driver_get_device_mig_mode_res_u.mode.error = ctx->err->code;
        res->driver_get_device_mig_mode_res_u.mode.current = current;
        res->driver_get_device_mig_mode_res_u.mode.pending = pending;
        return (true);
}

int
driver_get_device_max_mig_device_count(struct driver *ctx, struct driver_device *dev, unsigned int *count)
{
        // Initialize local variables.
        struct driver_get_device_max_mig_device_count_res res = {0};
        int rv = -1;

        // Initialize return values.
        *count = 0;

        // Make an RPC call to get the max count of MIG devices for this device.
        if (call_rpc(ctx, &res, driver_get_device_max_mig_device_count_1, (ptr_t)dev) < 0)
                goto fail;

        // Extract max MIG device count from the result of the RPC call and
        // populate the 'count' return value.
        *count = (unsigned int)res.driver_get_device_max_mig_device_count_res_u.count;

        // Set 'rv' to 0 to indicate success.
        rv = 0;

 fail:
        // Free the results of the RPC call and return.
        xdr_free((xdrproc_t)xdr_driver_get_device_max_mig_device_count_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_max_mig_device_count_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_max_mig_device_count_res *res, maybe_unused struct svc_req *req)
{
        // Initialize local variables.
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;

        // Clear out 'res' which will hold the result of this RPC call.
        memset(res, 0, sizeof(*res));

        // Grab a shorter reference to fields embedded in 'res' for the max MIG count.
        unsigned int *count = (unsigned int *)&res->driver_get_device_max_mig_device_count_res_u.count;

        // Call into NVML to get the max MIG count and assign it to '*count'.
        if (call_nvml(ctx, nvmlDeviceGetMaxMigDeviceCount, handle->nvml, count) < 0)
                goto fail;

        return (true);

 fail:
        // Populate the error in the result of the RPC call and return.
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_mig_device(struct driver *ctx, struct driver_device *dev, unsigned int idx, struct driver_device **mig_dev)
{
        // Initialize local variables.
        struct driver_get_device_mig_device_res res = {0};
        int rv = -1;

        // Initialize return values.
        *mig_dev = NULL;

        // Make an RPC call to get the MIG device t index 'idx' for this device.
        if (call_rpc(ctx, &res, driver_get_device_mig_device_1, (ptr_t)dev, idx) < 0)
                goto fail;

        // Extract the MIG device from the result of the RPC call and populate
        // the 'mig_dev' return value.
        *mig_dev = (struct driver_device *)res.driver_get_device_mig_device_res_u.dev;

        // Set 'rv' to 0 to indicate success.
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_mig_device_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_mig_device_1_svc(ptr_t ctxptr, ptr_t dev, u_int idx, driver_get_device_mig_device_res *res, maybe_unused struct svc_req *req)
{
        // Initialize local variables.
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;

        // Clear out 'res' which will hold the result of this RPC call.
        memset(res, 0, sizeof(*res));

        // Sanity check that we don't exceed MAX_MIG_DEVICES.
        if (idx >= MAX_MIG_DEVICES) {
                error_setx(ctx->err, "too many MIG devices");
                goto fail;
        }

        // Grab a shorter reference to mig field embedded in the device handle
        // for the MIG device.
        nvmlDevice_t *mig_dev = &handle->mig[idx].nvml;

        // Call into NVML to get the max MIG count and assign it to '*count'.
        if (call_nvml(ctx, nvmlDeviceGetMigDeviceHandleByIndex, handle->nvml, idx, mig_dev) < 0) {
                // If a device isn't found, then it's not an error, we just set
                // the result to NULL in our return value.
                switch (ctx->err->code) {
                case NVML_ERROR_NOT_FOUND:
                        res->driver_get_device_mig_device_res_u.dev = (ptr_t)NULL;
                        return (true);
                }
                // In all other cases, fail if the NVML call is not successful.
                goto fail;
        }

        // Assign the field embedded in the 'res' to point to the MIG device.
        res->driver_get_device_mig_device_res_u.dev = (ptr_t)&handle->mig[idx];

        return (true);

 fail:
        // Populate the error in the result of the RPC call and return.
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_gpu_instance_id(struct driver *ctx, struct driver_device *dev, unsigned int *id)
{
        // Initialize local variables.
        struct driver_get_device_gpu_instance_id_res res = {0};
        int rv = -1;

        // Initialize return values.
        *id = 0;

        // Make an RPC call to grab the instance ID of the GPU Instance.
        if (call_rpc(ctx, &res, driver_get_device_gpu_instance_id_1, (ptr_t)dev) < 0)
                goto fail;

        // Extract the id from the result of the RPC call and populate the 'id'
        // return value.
        *id = (unsigned int)res.driver_get_device_gpu_instance_id_res_u.id;

        // Set 'rv' to 0 to indicate success.
        rv = 0;

 fail:
        // Free the results of the RPC call and return.
        xdr_free((xdrproc_t)xdr_driver_get_device_gpu_instance_id_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_gpu_instance_id_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_gpu_instance_id_res *res, maybe_unused struct svc_req *req)
{
        // Initialize local variables.
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;

        // Clear out 'res' which will hold the result of this RPC call.
        memset(res, 0, sizeof(*res));

        // Grab a shorter reference to fields embedded in 'res' for the id.
        unsigned int *id = (unsigned int *)&res->driver_get_device_gpu_instance_id_res_u.id;

        // Call into NVML to get the GPU Instance Info.
        if (call_nvml(ctx, nvmlDeviceGetGpuInstanceId, handle->nvml, id) < 0)
                goto fail;

        return (true);

 fail:
        // Populate the error in the result of the RPC call and return.
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_compute_instance_id(struct driver *ctx, struct driver_device *dev, unsigned int *id)
{
        // Initialize local variables.
        struct driver_get_device_compute_instance_id_res res = {0};
        int rv = -1;

        // Initialize return values.
        *id = 0;

        // Make an RPC call to grab the instance ID of the Compute Instance.
        if (call_rpc(ctx, &res, driver_get_device_compute_instance_id_1, (ptr_t)dev) < 0)
                goto fail;

        // Extract the id from the result of the RPC call and populate the 'id'
        // return value.
        *id = (unsigned int)res.driver_get_device_compute_instance_id_res_u.id;

        // Set 'rv' to 0 to indicate success.
        rv = 0;

 fail:
        // Free the results of the RPC call and return.
        xdr_free((xdrproc_t)xdr_driver_get_device_compute_instance_id_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_compute_instance_id_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_compute_instance_id_res *res, maybe_unused struct svc_req *req)
{
        // Initialize local variables.
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;

        // Clear out 'res' which will hold the result of this RPC call.
        memset(res, 0, sizeof(*res));

        // Grab a shorter reference to fields embedded in 'res' for the id.
        unsigned int *id = (unsigned int *)&res->driver_get_device_compute_instance_id_res_u.id;

        // Call into NVML to get the Compute Instance Info.
        if (call_nvml(ctx, nvmlDeviceGetComputeInstanceId, handle->nvml, id) < 0)
                goto fail;

        return (true);
 fail:
        // Populate the error in the result of the RPC call and return.
        error_to_xdr(ctx->err, res);
        return (true);
}
