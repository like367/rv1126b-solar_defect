#ifndef RKNN_API_H
#define RKNN_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* board-verified ABI from /usr/include/rknn_api.h */
typedef uint64_t rknn_context;

#define RKNN_MAX_DIMS 16
#define RKNN_MAX_NAME_LEN 256

typedef enum {
    RKNN_TENSOR_NCHW = 0,
    RKNN_TENSOR_NHWC = 1,
    RKNN_TENSOR_UNDEFINED = 2,
} rknn_tensor_format;

typedef enum {
    RKNN_TENSOR_FLOAT32 = 0,
    RKNN_TENSOR_FLOAT16 = 1,
    RKNN_TENSOR_INT8 = 2,
    RKNN_TENSOR_UINT8 = 3,
    RKNN_TENSOR_INT16 = 4,
    RKNN_TENSOR_UINT16 = 5,
    RKNN_TENSOR_INT32 = 6,
    RKNN_TENSOR_UINT32 = 7,
    RKNN_TENSOR_INT64 = 8,
    RKNN_TENSOR_BOOL = 9,
} rknn_tensor_type;

typedef enum {
    RKNN_TENSOR_QNT_NONE = 0,
    RKNN_TENSOR_QNT_DFP = 1,
    RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC = 2,
} rknn_tensor_qnt_type;

typedef enum {
    RKNN_QUERY_IN_OUT_NUM  = 0,
    RKNN_QUERY_INPUT_ATTR  = 1,
    RKNN_QUERY_OUTPUT_ATTR = 2,
} rknn_query_cmd;

typedef struct _rknn_tensor_attr {
    uint32_t index;
    uint32_t n_dims;
    uint32_t dims[RKNN_MAX_DIMS];
    char name[RKNN_MAX_NAME_LEN];
    uint32_t n_elems;
    uint32_t size;
    rknn_tensor_format fmt;
    rknn_tensor_type type;
    rknn_tensor_qnt_type qnt_type;
    int8_t fl;
    uint32_t zp;
    float scale;
    uint32_t w_stride;
    uint32_t size_with_stride;
    uint8_t pass_through;
    uint32_t h_stride;
} rknn_tensor_attr;

typedef struct _rknn_input_output_num {
    uint32_t n_input;
    uint32_t n_output;
} rknn_input_output_num;

typedef struct _rknn_input {
    uint32_t index;
    void *buf;
    uint32_t size;
    uint8_t pass_through;
    rknn_tensor_type type;
    rknn_tensor_format fmt;
} rknn_input;

typedef struct _rknn_output {
    uint8_t want_float;
    uint8_t is_prealloc;
    uint32_t index;
    void *buf;
    uint32_t size;
} rknn_output;

typedef struct _rknn_sdk_version {
    char api_version[256];
    char drv_version[256];
} rknn_sdk_version;

typedef struct _rknn_run_extend {
    uint64_t frame_id;
    int32_t non_block;
    int32_t timeout_ms;
    int32_t fence_fd;
} rknn_run_extend;

#ifdef __cplusplus
}
#endif

#endif /* RKNN_API_H */
