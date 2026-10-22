#ifndef WECHATVOICEEXPORT_MD5_H
#define WECHATVOICEEXPORT_MD5_H

#include <stddef.h>
#include <stdint.h>

#define WECHATVOICEEXPORT_MD5_DIGEST_SIZE 16
#define WECHATVOICEEXPORT_MD5_HEX_SIZE 33

typedef struct wechatvoiceexport_md5_ctx_t {
    uint32_t state[4];
    uint64_t total_bytes;
    uint8_t buffer[64];
    size_t buffer_size;
} wechatvoiceexport_md5_ctx_t;

void wechatvoiceexport_md5_init(wechatvoiceexport_md5_ctx_t *ctx);
void wechatvoiceexport_md5_update(wechatvoiceexport_md5_ctx_t *ctx, const void *data, size_t size);
void wechatvoiceexport_md5_final(wechatvoiceexport_md5_ctx_t *ctx,
                                 uint8_t digest[WECHATVOICEEXPORT_MD5_DIGEST_SIZE]);
void wechatvoiceexport_md5_hex(const void *data, size_t size,
                               char out_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE]);

#endif /* WECHATVOICEEXPORT_MD5_H */
