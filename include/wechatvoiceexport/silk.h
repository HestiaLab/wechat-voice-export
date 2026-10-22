#ifndef WECHATVOICEEXPORT_SILK_H
#define WECHATVOICEEXPORT_SILK_H

/**
 * @file silk.h
 * @brief SILK v3 container parsing and decoding to 16-bit PCM.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>
#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default WeChat sampling rate in hertz. */
#define WECHATVOICEEXPORT_SILK_DEFAULT_RATE 24000u

/** Parsed layout of a SILK v3 container. */
typedef struct wechatvoiceexport_silk_layout_t {
    size_t header_size;    /**< 10 for `\x02#!SILK_V3`, otherwise 9. */
    size_t frame_count;    /**< Number of complete frames starting at the header. */
    size_t container_size; /**< Header plus complete frames; a valid payload end. */
    bool complete;         /**< True when the last frame ends exactly at the input size. */
} wechatvoiceexport_silk_layout_t;

/** Decoded audio. */
typedef struct wechatvoiceexport_pcm_t {
    int16_t *samples;     /**< Interleaved mono samples; owned by the result. */
    size_t sample_count;  /**< Number of samples. */
    uint32_t sample_rate; /**< Samples per second. */
    size_t frame_count;   /**< Number of SILK frames decoded. */
} wechatvoiceexport_pcm_t;

/**
 * @brief Inspect a SILK v3 container without decoding it.
 *
 * Parsing stops at the first zero-length frame or at the first frame that would
 * extend past `size`. A valid header with at least one complete frame counts as
 * success; use `complete` to require that the frames consume the whole input.
 *
 * @param data Container bytes; must not be NULL.
 * @param size Number of available bytes.
 * @param[out] out_layout Receives the parsed layout.
 * @return OK, INVALID_ARGUMENT, or DECODE (not a SILK v3 container).
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_silk_inspect(
    const uint8_t *data, size_t size, wechatvoiceexport_silk_layout_t *out_layout);

/**
 * @brief Decode a complete SILK v3 container to signed 16-bit PCM.
 * @param data Payload bytes including the 9- or 10-byte WeChat header.
 * @param size Number of payload bytes.
 * @param sample_rate Output rate in hertz: 8000, 12000, 16000, or 24000.
 * @param[out] out_pcm Receives owned samples on success.
 * @return OK, INVALID_ARGUMENT, DECODE, or NOMEM.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_silk_decode(
    const uint8_t *data, size_t size, uint32_t sample_rate, wechatvoiceexport_pcm_t *out_pcm);

/**
 * @brief Release decoded samples and zero the result.
 * @param pcm Result to release; NULL is allowed.
 */
WECHATVOICEEXPORT_API void wechatvoiceexport_pcm_free(wechatvoiceexport_pcm_t *pcm);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_SILK_H */
