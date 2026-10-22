#ifndef WECHATVOICEEXPORT_CAPTURE_H
#define WECHATVOICEEXPORT_CAPTURE_H

/**
 * @file capture.h
 * @brief Read-only discovery of decrypted SILK payloads in WeChat process memory.
 *
 * The portable extraction entry point is available on every platform and is the
 * unit-test seam; process enumeration and memory reading are Windows-only and
 * return WECHATVOICEEXPORT_STATUS_UNSUPPORTED elsewhere.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>
#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Process identifiers returned by wechatvoiceexport_capture_list_processes(). */
typedef struct wechatvoiceexport_pid_list_t {
    uint32_t *items; /**< Owned array. */
    size_t count;    /**< Number of processes. */
} wechatvoiceexport_pid_list_t;

/** One validated SILK payload found in memory. */
typedef struct wechatvoiceexport_candidate_t {
    uint8_t *payload;   /**< Owned copy of the complete container. */
    size_t size;        /**< Payload size in bytes. */
    size_t frame_count; /**< Number of SILK frames. */
    char md5_hex[33];   /**< Lowercase MD5 of the payload. */
} wechatvoiceexport_candidate_t;

/** Deduplicated candidate list. */
typedef struct wechatvoiceexport_candidate_list_t {
    wechatvoiceexport_candidate_t *items; /**< Owned array. */
    size_t count;                         /**< Number of candidates. */
} wechatvoiceexport_candidate_list_t;

/**
 * @brief Enumerate running processes by image name (Windows only).
 * @param image_name Executable name such as `Weixin.exe`; case-insensitive.
 * @param[out] out_list Receives an owned list; empty when nothing matched.
 * @return OK, INVALID_ARGUMENT, IO, NOMEM, or UNSUPPORTED.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_capture_list_processes(
    const char *image_name, wechatvoiceexport_pid_list_t *out_list);

/**
 * @brief Scan one process for SILK payloads whose size is in the given range.
 *
 * Only committed, readable, non-guard pages are read, using
 * `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`. Nothing is written to the
 * target process and no dump is produced.
 *
 * @param pid Target process identifier.
 * @param min_payload Inclusive minimum container size.
 * @param max_payload Inclusive maximum container size.
 * @param[in,out] out_list Appends deduplicated candidates; must not be NULL.
 * @return OK, INVALID_ARGUMENT, IO, NOMEM, or UNSUPPORTED.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t
wechatvoiceexport_capture_scan_process(uint32_t pid, uint64_t min_payload, uint64_t max_payload,
                                       wechatvoiceexport_candidate_list_t *out_list);

/**
 * @brief Extract validated SILK payloads from a contiguous byte image.
 *
 * This is the portable core of the memory scan. A header occurrence is accepted
 * only when at least one frame parses and the frame chain ends at a length in
 * `[min_payload, max_payload]`. The largest in-range boundary is used.
 *
 * @param buffer Bytes to inspect; may contain arbitrary surrounding data.
 * @param size Number of bytes in `buffer`.
 * @param min_payload Inclusive minimum container size.
 * @param max_payload Inclusive maximum container size.
 * @param[in,out] out_list Appends deduplicated candidates; must not be NULL.
 * @return OK, INVALID_ARGUMENT, or NOMEM.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_capture_extract(
    const uint8_t *buffer, size_t size, uint64_t min_payload, uint64_t max_payload,
    wechatvoiceexport_candidate_list_t *out_list);

/**
 * @brief Release a PID list and zero it.
 * @param list List to release; NULL is allowed.
 */
WECHATVOICEEXPORT_API void wechatvoiceexport_capture_pids_free(wechatvoiceexport_pid_list_t *list);

/**
 * @brief Release a candidate list and zero it.
 * @param list List to release; NULL is allowed.
 */
WECHATVOICEEXPORT_API void
wechatvoiceexport_capture_candidates_free(wechatvoiceexport_candidate_list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_CAPTURE_H */
