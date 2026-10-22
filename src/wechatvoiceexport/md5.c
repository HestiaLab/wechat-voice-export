#include "md5.h"

#include <string.h>

static const uint32_t WECHATVOICEEXPORT_MD5_K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u,
    0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u,
    0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du,
    0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
    0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
    0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u,
    0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu,
    0xeb86d391u};

static const uint8_t WECHATVOICEEXPORT_MD5_S[64] = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21};

static uint32_t wechatvoiceexport_md5_rotl(uint32_t value, unsigned shift) {
    return (value << shift) | (value >> (32u - shift));
}

static void wechatvoiceexport_md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t words[16];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    for (size_t i = 0; i < 16; ++i) {
        size_t offset = i * 4;
        words[i] = (uint32_t)block[offset] | ((uint32_t)block[offset + 1] << 8) |
                   ((uint32_t)block[offset + 2] << 16) | ((uint32_t)block[offset + 3] << 24);
    }

    for (unsigned i = 0; i < 64; ++i) {
        uint32_t f;
        unsigned g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5u * i + 1u) % 16u;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3u * i + 5u) % 16u;
        } else {
            f = c ^ (b | ~d);
            g = (7u * i) % 16u;
        }
        f = f + a + WECHATVOICEEXPORT_MD5_K[i] + words[g];
        a = d;
        d = c;
        c = b;
        b = b + wechatvoiceexport_md5_rotl(f, (unsigned)WECHATVOICEEXPORT_MD5_S[i]);
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void wechatvoiceexport_md5_init(wechatvoiceexport_md5_ctx_t *ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu;
    ctx->state[3] = 0x10325476u;
    ctx->total_bytes = 0;
    ctx->buffer_size = 0;
}

void wechatvoiceexport_md5_update(wechatvoiceexport_md5_ctx_t *ctx, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    size_t offset = 0;

    ctx->total_bytes += (uint64_t)size;

    if (ctx->buffer_size > 0) {
        size_t needed = 64 - ctx->buffer_size;
        size_t take = size < needed ? size : needed;
        memcpy(ctx->buffer + ctx->buffer_size, bytes, take);
        ctx->buffer_size += take;
        offset += take;
        if (ctx->buffer_size == 64) {
            wechatvoiceexport_md5_transform(ctx->state, ctx->buffer);
            ctx->buffer_size = 0;
        }
    }

    while (size - offset >= 64) {
        wechatvoiceexport_md5_transform(ctx->state, bytes + offset);
        offset += 64;
    }

    if (offset < size) {
        ctx->buffer_size = size - offset;
        memcpy(ctx->buffer, bytes + offset, ctx->buffer_size);
    }
}

void wechatvoiceexport_md5_final(wechatvoiceexport_md5_ctx_t *ctx,
                                 uint8_t digest[WECHATVOICEEXPORT_MD5_DIGEST_SIZE]) {
    uint64_t bit_count = ctx->total_bytes * 8u;
    uint8_t padding[64] = {0x80};
    size_t padding_size = ctx->buffer_size < 56 ? 56 - ctx->buffer_size : 120 - ctx->buffer_size;

    wechatvoiceexport_md5_update(ctx, padding, padding_size);

    uint8_t length_bytes[8];
    for (size_t i = 0; i < 8; ++i) {
        length_bytes[i] = (uint8_t)(bit_count >> (8u * i));
    }
    wechatvoiceexport_md5_update(ctx, length_bytes, sizeof length_bytes);

    for (size_t i = 0; i < 4; ++i) {
        digest[i * 4] = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }

    memset(ctx, 0, sizeof *ctx);
}

void wechatvoiceexport_md5_hex(const void *data, size_t size,
                               char out_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    uint8_t digest[WECHATVOICEEXPORT_MD5_DIGEST_SIZE];
    wechatvoiceexport_md5_ctx_t ctx;

    wechatvoiceexport_md5_init(&ctx);
    if (size > 0) {
        wechatvoiceexport_md5_update(&ctx, data, size);
    }
    wechatvoiceexport_md5_final(&ctx, digest);

    for (size_t i = 0; i < WECHATVOICEEXPORT_MD5_DIGEST_SIZE; ++i) {
        out_hex[i * 2] = digits[digest[i] >> 4];
        out_hex[i * 2 + 1] = digits[digest[i] & 0x0fu];
    }
    out_hex[32] = '\0';
}
