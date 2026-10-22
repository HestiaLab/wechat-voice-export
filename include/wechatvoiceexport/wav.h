#ifndef WECHATVOICEEXPORT_WAV_H
#define WECHATVOICEEXPORT_WAV_H

/**
 * @file wav.h
 * @brief Standard RIFF/WAVE writer for mono 16-bit PCM.
 */

#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>
#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write mono 16-bit PCM as a canonical 44-byte-header WAV file.
 *
 * The destination is created or truncated. UTF-8 paths are supported on every
 * platform.
 *
 * @param path Destination path; must not be NULL.
 * @param samples Sample buffer; must not be NULL when `sample_count` is nonzero.
 * @param sample_count Number of samples.
 * @param sample_rate Sampling rate in hertz; must be nonzero.
 * @return OK, INVALID_ARGUMENT, IO, or NOMEM.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_wav_write(const char *path,
                                                                             const int16_t *samples,
                                                                             size_t sample_count,
                                                                             uint32_t sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_WAV_H */
