/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

%#pragma GCC diagnostic ignored "-Wmissing-prototypes"
%#pragma GCC diagnostic ignored "-Wsign-conversion"
%#pragma GCC diagnostic ignored "-Wunused-variable"

typedef int64_t ptr_t;

union driver_init_res switch (int errcode) {
        case 0:
                void;
        default:
                string errmsg<>;
};

union driver_shutdown_res switch (int errcode) {
        case 0:
                void;
        default:
                string errmsg<>;
};

union driver_get_rm_version_res switch (int errcode) {
        case 0:
                string vers<>;
        default:
                string errmsg<>;
};

struct driver_cuda_version {
        unsigned int major;
        unsigned int minor;
};

union driver_get_cuda_version_res switch (int errcode) {
        case 0:
                driver_cuda_version vers;
        default:
                string errmsg<>;
};

struct driver_device_arch {
        unsigned int major;
        unsigned int minor;
};

union driver_get_device_arch_res switch (int errcode) {
        case 0:
                driver_device_arch arch;
        default:
                string errmsg<>;
};

union driver_get_device_count_res switch (int errcode) {
        case 0:
                unsigned int count;
        default:
                string errmsg<>;
};

union driver_get_device_res switch (int errcode) {
        case 0:
                ptr_t dev;
        default:
                string errmsg<>;
};

union driver_get_device_minor_res switch (int errcode) {
        case 0:
                unsigned int minor;
        default:
                string errmsg<>;
};

union driver_get_device_busid_res switch (int errcode) {
        case 0:
                string busid<>;
        default:
                string errmsg<>;
};

union driver_get_device_uuid_res switch (int errcode) {
        case 0:
                string uuid<>;
        default:
                string errmsg<>;
};

union driver_get_device_model_res switch (int errcode) {
        case 0:
                string model<>;
        default:
                string errmsg<>;
};

union driver_get_device_brand_res switch (int errcode) {
        case 0:
                string brand<>;
        default:
                string errmsg<>;
};

struct driver_device_mig_mode {
        int error;
        unsigned int current;
        unsigned int pending;
};

union driver_get_device_mig_mode_res switch (int errcode) {
        case 0:
                driver_device_mig_mode mode;
        default:
                string errmsg<>;
};

union driver_get_device_max_mig_device_count_res switch (int errcode) {
        case 0:
                unsigned int count;
        default:
                string errmsg<>;
};

union driver_get_device_mig_device_res switch (int errcode) {
        case 0:
                ptr_t dev;
        default:
                string errmsg<>;
};

union driver_get_device_gpu_instance_id_res switch (int errcode) {
        case 0:
               unsigned int id;
        default:
                string errmsg<>;
};

union driver_get_device_compute_instance_id_res switch (int errcode) {
        case 0:
               unsigned int id;
        default:
                string errmsg<>;
};

program DRIVER_PROGRAM {
        version DRIVER_VERSION {
                driver_init_res DRIVER_INIT(ptr_t) = 1;
                driver_shutdown_res DRIVER_SHUTDOWN(ptr_t) = 2;
                driver_get_rm_version_res DRIVER_GET_RM_VERSION(ptr_t) = 3;
                driver_get_cuda_version_res DRIVER_GET_CUDA_VERSION(ptr_t) = 4;
                driver_get_device_count_res DRIVER_GET_DEVICE_COUNT(ptr_t) = 5;
                driver_get_device_res DRIVER_GET_DEVICE(ptr_t, unsigned int) = 6;
                driver_get_device_minor_res DRIVER_GET_DEVICE_MINOR(ptr_t, ptr_t) = 7;
                driver_get_device_busid_res DRIVER_GET_DEVICE_BUSID(ptr_t, ptr_t) = 8;
                driver_get_device_uuid_res DRIVER_GET_DEVICE_UUID(ptr_t, ptr_t) = 9;
                driver_get_device_arch_res DRIVER_GET_DEVICE_ARCH(ptr_t, ptr_t) = 10;
                driver_get_device_model_res DRIVER_GET_DEVICE_MODEL(ptr_t, ptr_t) = 11;
                driver_get_device_brand_res DRIVER_GET_DEVICE_BRAND(ptr_t, ptr_t) = 12;
                driver_get_device_mig_mode_res DRIVER_GET_DEVICE_MIG_MODE(ptr_t, ptr_t) = 13;
                driver_get_device_max_mig_device_count_res DRIVER_GET_DEVICE_MAX_MIG_DEVICE_COUNT(ptr_t, ptr_t) = 14;
                driver_get_device_mig_device_res DRIVER_GET_DEVICE_MIG_DEVICE(ptr_t, ptr_t, unsigned int) = 15;
                driver_get_device_gpu_instance_id_res DRIVER_GET_DEVICE_GPU_INSTANCE_ID(ptr_t, ptr_t) = 16;
                driver_get_device_compute_instance_id_res DRIVER_GET_DEVICE_COMPUTE_INSTANCE_ID(ptr_t, ptr_t) = 17;
        } = 1;
} = 1;

#ifdef WITH_NVCGO
union nvcgo_init_res switch (int errcode) {
        case 0:
                void;
        default:
                string errmsg<>;
};

union nvcgo_shutdown_res switch (int errcode) {
        case 0:
                void;
        default:
                string errmsg<>;
};

program NVCGO_PROGRAM {
        version NVCGO_VERSION {
                nvcgo_init_res NVCGO_INIT(ptr_t) = 1;
                nvcgo_shutdown_res NVCGO_SHUTDOWN(ptr_t) = 2;
        } = 1;
} = 2;
#endif
