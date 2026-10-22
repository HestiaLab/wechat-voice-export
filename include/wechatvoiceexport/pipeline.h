#ifndef WECHATVOICEEXPORT_PIPELINE_H
#define WECHATVOICEEXPORT_PIPELINE_H

/**
 * @file pipeline.h
 * @brief Decode a captured SILK payload and write WAV/MP3 output files.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>
#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Export configuration. All strings are borrowed and must stay valid. */
typedef struct wechatvoiceexport_export_options_t {
    const char *output_dir;  /**< Destination directory; created when missing. */
    const char *base_name;   /**< File stem without extension; no path separators. */
    const char *data_root;   /**< Optional WeChat root that output must stay outside. */
    const char *ffmpeg_path; /**< Optional ffmpeg executable; NULL uses `ffmpeg`. */
    bool write_mp3;          /**< Invoke ffmpeg when true. */
    bool force;              /**< Overwrite existing output files when true. */
} wechatvoiceexport_export_options_t;

/** Paths and audio statistics produced by an export. */
typedef struct wechatvoiceexport_export_result_t {
    char *wav_path;       /**< Owned path to the written WAV, or NULL. */
    char *mp3_path;       /**< Owned path to the written MP3, or NULL. */
    size_t sample_count;  /**< Number of PCM samples per channel. */
    size_t frame_count;   /**< Number of decoded SILK frames. */
    uint32_t sample_rate; /**< Output sampling rate in hertz. */
} wechatvoiceexport_export_result_t;

/**
 * @brief Decode a SILK payload and write a WAV, optionally followed by an MP3.
 *
 * The WAV is always written before ffmpeg runs. If ffmpeg is missing or fails,
 * the WAV is kept, `out_result` describes it, and MP3 is returned.
 *
 * @param payload Complete SILK container including its header.
 * @param payload_size Number of payload bytes.
 * @param options Export options; must not be NULL.
 * @param[out] out_result Receives paths and statistics on success, and on MP3
 * failure. Initialized to zero on entry.
 * @return OK, INVALID_ARGUMENT (path, containment, or overwrite conflict), IO,
 * NOMEM, DECODE, or MP3.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_export_silk(
    const uint8_t *payload, size_t payload_size, const wechatvoiceexport_export_options_t *options,
    wechatvoiceexport_export_result_t *out_result);

/**
 * @brief Release result paths and zero the result.
 * @param result Result to release; NULL is allowed.
 */
WECHATVOICEEXPORT_API void
wechatvoiceexport_export_result_free(wechatvoiceexport_export_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_PIPELINE_H */
