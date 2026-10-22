#ifndef WECHATVOICEEXPORT_ERROR_H
#define WECHATVOICEEXPORT_ERROR_H

/**
 * @file error.h
 * @brief Status codes shared by the public API.
 */

#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result of a library operation. */
typedef enum wechatvoiceexport_status_t {
    WECHATVOICEEXPORT_STATUS_OK = 0,
    WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT = 1,
    WECHATVOICEEXPORT_STATUS_IO = 2,
    WECHATVOICEEXPORT_STATUS_NOT_FOUND = 3,
    WECHATVOICEEXPORT_STATUS_NOMEM = 4,
    WECHATVOICEEXPORT_STATUS_UNSUPPORTED = 5,
    WECHATVOICEEXPORT_STATUS_DECODE = 6,
    WECHATVOICEEXPORT_STATUS_AMBIGUOUS = 7,
    WECHATVOICEEXPORT_STATUS_MP3 = 8
} wechatvoiceexport_status_t;

/**
 * @brief Return a short English description of a status code.
 * @param status Status value to describe.
 * @return A static string; never NULL, even for unknown values.
 */
WECHATVOICEEXPORT_API const char *
wechatvoiceexport_status_string(wechatvoiceexport_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_ERROR_H */
