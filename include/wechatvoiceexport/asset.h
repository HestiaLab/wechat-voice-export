#ifndef WECHATVOICEEXPORT_ASSET_H
#define WECHATVOICEEXPORT_ASSET_H

/**
 * @file asset.h
 * @brief Discovery of favorited WeChat voice assets on disk.
 *
 * Assets are discovered only under `business/favorite`; the persistent copy
 * under `business/xeditor/XEditorBackup/Resources` is used solely to
 * corroborate an already discovered asset.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>
#include <wechatvoiceexport/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size of a lowercase hexadecimal MD5 string, including the terminator. */
#define WECHATVOICEEXPORT_MD5_HEX_SIZE 33
/** Size of the short hash shown to users, including the terminator. */
#define WECHATVOICEEXPORT_SHORT_HASH_SIZE 9

/** One logical voice asset, deduplicated by ciphertext MD5. */
typedef struct wechatvoiceexport_asset_t {
    char *path; /**< Representative path; owned by the asset list. */
    char md5_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE]; /**< Lowercase MD5 of the file. */
    uint64_t ciphertext_size;                     /**< Size of the encrypted file in bytes. */
    size_t source_count;                          /**< Number of identical files discovered. */
    bool has_persistent_copy;                     /**< A verified XEditorBackup copy exists. */
} wechatvoiceexport_asset_t;

/** Result of a data-root scan. */
typedef struct wechatvoiceexport_asset_list_t {
    wechatvoiceexport_asset_t *items; /**< Owned array, sorted by MD5. */
    size_t count;                     /**< Number of logical assets. */
    size_t ignored_too_small;         /**< Files skipped because the ciphertext was < 17 bytes. */
} wechatvoiceexport_asset_list_t;

/**
 * @brief Resolve the WeChat data root.
 *
 * Resolution order: `override_path` when non-NULL, the Windows Documents known
 * folder followed by `xwechat_files`, then `$HOME/Documents/xwechat_files`.
 *
 * @param override_path Optional explicit path; may be NULL.
 * @param[out] out_root Receives an owned string. On success it is the resolved
 * directory. On WECHATVOICEEXPORT_STATUS_NOT_FOUND it is the last path tried,
 * suitable for an error message. Must not be NULL.
 * @return OK, INVALID_ARGUMENT, NOT_FOUND, or NOMEM.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t
wechatvoiceexport_data_root_resolve(const char *override_path, char **out_root);

/**
 * @brief Scan a data root for favorited voice assets.
 *
 * The root may be `xwechat_files` or a single account directory. Duplicate
 * content is merged into one logical asset and `source_count` is incremented.
 *
 * @param data_root Existing directory to scan. Must not be NULL.
 * @param[out] out_list Receives the owned asset list; must not be NULL.
 * @return OK (possibly with zero assets), INVALID_ARGUMENT, NOT_FOUND (root is
 * not a readable directory), IO, or NOMEM.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t
wechatvoiceexport_assets_scan(const char *data_root, wechatvoiceexport_asset_list_t *out_list);

/**
 * @brief Release the contents of an asset list and zero it.
 * @param list List to release; NULL is allowed.
 */
WECHATVOICEEXPORT_API void wechatvoiceexport_assets_free(wechatvoiceexport_asset_list_t *list);

/**
 * @brief Resolve a one-based index or a unique MD5 prefix.
 *
 * A selector consisting only of decimal digits is always interpreted as a
 * one-based index. Otherwise it is matched case-insensitively against the
 * beginning of each asset's MD5.
 *
 * @param list Asset list to search; must not be NULL.
 * @param selector Non-empty selector; must not be NULL.
 * @param[out] out_index Receives the zero-based asset index on success.
 * @return OK, NOT_FOUND, AMBIGUOUS, or INVALID_ARGUMENT.
 */
WECHATVOICEEXPORT_API wechatvoiceexport_status_t wechatvoiceexport_asset_find(
    const wechatvoiceexport_asset_list_t *list, const char *selector, size_t *out_index);

/**
 * @brief Compute the expected plaintext-size range for a ciphertext.
 *
 * PKCS#7 padding removes between 1 and 16 bytes. The lower bound is clamped to
 * the 10-byte SILK header.
 *
 * @param ciphertext_size Encrypted file size in bytes.
 * @param[out] out_min Receives the inclusive minimum plaintext size.
 * @param[out] out_max Receives the inclusive maximum plaintext size.
 * @return False when the ciphertext is too small (< 17 bytes) or an output
 * pointer is NULL.
 */
WECHATVOICEEXPORT_API bool wechatvoiceexport_asset_expected_plain_range(uint64_t ciphertext_size,
                                                                        uint64_t *out_min,
                                                                        uint64_t *out_max);

#ifdef __cplusplus
}
#endif

#endif /* WECHATVOICEEXPORT_ASSET_H */
