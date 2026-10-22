#include <wechatvoiceexport/asset.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "md5.h"

#if defined(_WIN32)
#include <objbase.h>
#include <shlobj.h>
#include <windows.h>
#endif

typedef struct wechatvoiceexport_asset_scan_context_t {
    wechatvoiceexport_asset_list_t *list;
    const char *account_root;
    const char *extension;
} wechatvoiceexport_asset_scan_context_t;

static char *wechatvoiceexport_asset_strdup(const char *value) {
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, value, length + 1);
    }
    return copy;
}

static bool wechatvoiceexport_asset_is_hex(const char *value, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

#if defined(_WIN32)
static char *wechatvoiceexport_asset_wide_to_utf8(const wchar_t *value) {
    int length;
    char *utf8;

    length = WideCharToMultiByte(CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL);
    if (length <= 0) {
        return NULL;
    }
    utf8 = (char *)malloc((size_t)length);
    if (utf8 == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8, length, NULL, NULL) <= 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}
#endif

static wechatvoiceexport_status_t wechatvoiceexport_asset_default_candidates(char ***out_paths,
                                                                             size_t *out_count) {
    char **paths;
    size_t count = 0;

    *out_paths = NULL;
    *out_count = 0;
    paths = (char **)malloc(2 * sizeof(char *));
    if (paths == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    paths[0] = NULL;
    paths[1] = NULL;

#if defined(_WIN32)
    {
        char *base = NULL;
        PWSTR documents = NULL;
        if (SHGetKnownFolderPath(&FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &documents) == S_OK &&
            documents != NULL) {
            base = wechatvoiceexport_asset_wide_to_utf8(documents);
            CoTaskMemFree(documents);
        }
        if (base != NULL) {
            if (wechatvoiceexport_fs_join(base, "xwechat_files", &paths[count]) !=
                WECHATVOICEEXPORT_STATUS_OK) {
                free(base);
                wechatvoiceexport_fs_free_strings(paths, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            ++count;
            free(base);
        } else {
            const char *profile = getenv("USERPROFILE");
            if (profile != NULL && profile[0] != '\0') {
                char *documents_path = NULL;
                if (wechatvoiceexport_fs_join(profile, "Documents", &documents_path) !=
                    WECHATVOICEEXPORT_STATUS_OK) {
                    wechatvoiceexport_fs_free_strings(paths, count);
                    return WECHATVOICEEXPORT_STATUS_NOMEM;
                }
                if (wechatvoiceexport_fs_join(documents_path, "xwechat_files", &paths[count]) !=
                    WECHATVOICEEXPORT_STATUS_OK) {
                    free(documents_path);
                    wechatvoiceexport_fs_free_strings(paths, count);
                    return WECHATVOICEEXPORT_STATUS_NOMEM;
                }
                ++count;
                free(documents_path);
            }
        }
        {
            const char *profile = getenv("USERPROFILE");
            if (profile != NULL && profile[0] != '\0') {
                char *documents_path = NULL;
                char *candidate = NULL;
                if (wechatvoiceexport_fs_join(profile, "Documents", &documents_path) !=
                        WECHATVOICEEXPORT_STATUS_OK ||
                    wechatvoiceexport_fs_join(documents_path, "xwechat_files", &candidate) !=
                        WECHATVOICEEXPORT_STATUS_OK) {
                    free(documents_path);
                    wechatvoiceexport_fs_free_strings(paths, count);
                    return WECHATVOICEEXPORT_STATUS_NOMEM;
                }
                free(documents_path);
                if (count == 0 || strcmp(paths[0], candidate) != 0) {
                    paths[count++] = candidate;
                } else {
                    free(candidate);
                }
            }
        }
    }
#else
    {
        const char *home = getenv("HOME");
        if (home != NULL && home[0] != '\0') {
            char *documents = NULL;
            if (wechatvoiceexport_fs_join(home, "Documents", &documents) !=
                    WECHATVOICEEXPORT_STATUS_OK ||
                wechatvoiceexport_fs_join(documents, "xwechat_files", &paths[count]) !=
                    WECHATVOICEEXPORT_STATUS_OK) {
                free(documents);
                wechatvoiceexport_fs_free_strings(paths, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            ++count;
            free(documents);
        }
    }
#endif

    if (count == 0) {
        free(paths);
        return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
    }
    *out_paths = paths;
    *out_count = count;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t wechatvoiceexport_data_root_resolve(const char *override_path,
                                                               char **out_root) {
    if (out_root == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_root = NULL;

    if (override_path != NULL) {
        char *copy;
        if (override_path[0] == '\0') {
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
        copy = wechatvoiceexport_asset_strdup(override_path);
        if (copy == NULL) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        if (!wechatvoiceexport_fs_dir_exists(copy)) {
            *out_root = copy;
            return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
        }
        *out_root = copy;
        return WECHATVOICEEXPORT_STATUS_OK;
    }

    {
        char **candidates = NULL;
        size_t count = 0;
        wechatvoiceexport_status_t status =
            wechatvoiceexport_asset_default_candidates(&candidates, &count);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            return status;
        }
        for (size_t i = 0; i < count; ++i) {
            if (wechatvoiceexport_fs_dir_exists(candidates[i])) {
                char *selected = candidates[i];
                candidates[i] = NULL;
                wechatvoiceexport_fs_free_strings(candidates, count);
                *out_root = selected;
                return WECHATVOICEEXPORT_STATUS_OK;
            }
        }
        *out_root = candidates[count - 1];
        candidates[count - 1] = NULL;
        wechatvoiceexport_fs_free_strings(candidates, count);
    }
    return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
}

static bool wechatvoiceexport_asset_has_persistent_copy(const char *account_root,
                                                        const char *md5_hex, uint64_t asset_size) {
    char *resources = NULL;
    char *path = NULL;
    uint8_t *data = NULL;
    size_t size = 0;
    bool found = false;

    if (wechatvoiceexport_fs_join(account_root, "business/xeditor/XEditorBackup/Resources",
                                  &resources) != WECHATVOICEEXPORT_STATUS_OK) {
        return false;
    }
    if (wechatvoiceexport_fs_join(resources, md5_hex, &path) != WECHATVOICEEXPORT_STATUS_OK) {
        free(resources);
        return false;
    }
    free(resources);
    if (!wechatvoiceexport_fs_file_exists(path)) {
        free(path);
        return false;
    }
    /* WeChat may re-encrypt the resource copy, so byte equality is not expected. */
    if (wechatvoiceexport_fs_read_file(path, &data, &size) == WECHATVOICEEXPORT_STATUS_OK) {
        found = (uint64_t)size == asset_size;
        free(data);
    }
    free(path);
    return found;
}

static wechatvoiceexport_status_t
wechatvoiceexport_asset_add(wechatvoiceexport_asset_scan_context_t *context, const char *path,
                            size_t size, const char *md5_hex) {
    wechatvoiceexport_asset_list_t *list = context->list;
    bool has_copy =
        wechatvoiceexport_asset_has_persistent_copy(context->account_root, md5_hex, (uint64_t)size);

    for (size_t i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i].md5_hex, md5_hex) == 0) {
            ++list->items[i].source_count;
            if (has_copy) {
                list->items[i].has_persistent_copy = true;
            }
            if (strcmp(path, list->items[i].path) < 0) {
                char *copy = wechatvoiceexport_asset_strdup(path);
                if (copy == NULL) {
                    return WECHATVOICEEXPORT_STATUS_NOMEM;
                }
                free(list->items[i].path);
                list->items[i].path = copy;
            }
            return WECHATVOICEEXPORT_STATUS_OK;
        }
    }

    {
        wechatvoiceexport_asset_t *grown = (wechatvoiceexport_asset_t *)realloc(
            list->items, (list->count + 1) * sizeof(*list->items));
        wechatvoiceexport_asset_t *asset;
        if (grown == NULL) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        list->items = grown;
        asset = &list->items[list->count];
        memset(asset, 0, sizeof *asset);
        asset->path = wechatvoiceexport_asset_strdup(path);
        if (asset->path == NULL) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        memcpy(asset->md5_hex, md5_hex, WECHATVOICEEXPORT_MD5_HEX_SIZE);
        asset->ciphertext_size = (uint64_t)size;
        asset->source_count = 1;
        asset->has_persistent_copy = has_copy;
        ++list->count;
    }
    return WECHATVOICEEXPORT_STATUS_OK;
}

static wechatvoiceexport_status_t wechatvoiceexport_asset_visit(const char *path,
                                                                void *context_ptr) {
    wechatvoiceexport_asset_scan_context_t *context =
        (wechatvoiceexport_asset_scan_context_t *)context_ptr;
    uint8_t *data = NULL;
    size_t size = 0;
    char md5_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    wechatvoiceexport_status_t status;

    if (!wechatvoiceexport_fs_has_extension(path, context->extension)) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    status = wechatvoiceexport_fs_read_file(path, &data, &size);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    if (size < 17) {
        ++context->list->ignored_too_small;
        free(data);
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    wechatvoiceexport_md5_hex(data, size, md5_hex);
    status = wechatvoiceexport_asset_add(context, path, size, md5_hex);
    free(data);
    return status;
}

static wechatvoiceexport_status_t
wechatvoiceexport_asset_scan_directory(wechatvoiceexport_asset_list_t *list,
                                       const char *account_root, const char *directory,
                                       const char *extension) {
    wechatvoiceexport_asset_scan_context_t context;

    if (!wechatvoiceexport_fs_dir_exists(directory)) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    context.list = list;
    context.account_root = account_root;
    context.extension = extension;
    return wechatvoiceexport_fs_walk(directory, true, wechatvoiceexport_asset_visit, &context);
}

static wechatvoiceexport_status_t
wechatvoiceexport_asset_scan_account(wechatvoiceexport_asset_list_t *list,
                                     const char *account_root) {
    char *note_cache = NULL;
    char *temp = NULL;
    wechatvoiceexport_status_t status;

    status =
        wechatvoiceexport_fs_join(account_root, "business/favorite/temp/NoteCache", &note_cache);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    status = wechatvoiceexport_asset_scan_directory(list, account_root, note_cache, ".silk");
    free(note_cache);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }

    status = wechatvoiceexport_fs_join(account_root, "business/favorite/temp", &temp);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    status = wechatvoiceexport_asset_scan_directory(list, account_root, temp, ".silk_temp");
    free(temp);
    return status;
}

static int wechatvoiceexport_asset_compare(const void *left, const void *right) {
    const wechatvoiceexport_asset_t *left_asset = (const wechatvoiceexport_asset_t *)left;
    const wechatvoiceexport_asset_t *right_asset = (const wechatvoiceexport_asset_t *)right;
    int order = strcmp(left_asset->md5_hex, right_asset->md5_hex);
    if (order != 0) {
        return order;
    }
    return strcmp(left_asset->path, right_asset->path);
}

wechatvoiceexport_status_t wechatvoiceexport_assets_scan(const char *data_root,
                                                         wechatvoiceexport_asset_list_t *out_list) {
    char **accounts = NULL;
    size_t account_count = 0;
    char *favorite = NULL;
    wechatvoiceexport_status_t status;

    if (data_root == NULL || out_list == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_list, 0, sizeof *out_list);
    if (!wechatvoiceexport_fs_dir_exists(data_root)) {
        return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
    }

    status = wechatvoiceexport_fs_join(data_root, "business/favorite", &favorite);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    if (wechatvoiceexport_fs_dir_exists(favorite)) {
        accounts = (char **)malloc(sizeof(char *));
        if (accounts == NULL) {
            free(favorite);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        accounts[0] = wechatvoiceexport_asset_strdup(data_root);
        if (accounts[0] == NULL) {
            free(accounts);
            free(favorite);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        account_count = 1;
    }
    free(favorite);

    if (account_count == 0) {
        char **subdirs = NULL;
        size_t subdir_count = 0;
        status = wechatvoiceexport_fs_list_subdirs(data_root, &subdirs, &subdir_count);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            return status;
        }
        accounts = (char **)malloc((subdir_count == 0 ? 1 : subdir_count) * sizeof(char *));
        if (accounts == NULL) {
            wechatvoiceexport_fs_free_strings(subdirs, subdir_count);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        for (size_t i = 0; i < subdir_count; ++i) {
            char *account_favorite = NULL;
            char *absolute = NULL;
            if (wechatvoiceexport_fs_join(data_root, subdirs[i], &absolute) !=
                WECHATVOICEEXPORT_STATUS_OK) {
                status = WECHATVOICEEXPORT_STATUS_NOMEM;
                break;
            }
            status = wechatvoiceexport_fs_join(absolute, "business/favorite", &account_favorite);
            if (status == WECHATVOICEEXPORT_STATUS_OK &&
                wechatvoiceexport_fs_dir_exists(account_favorite)) {
                accounts[account_count++] = absolute;
                absolute = NULL;
            }
            free(account_favorite);
            free(absolute);
            if (status != WECHATVOICEEXPORT_STATUS_OK) {
                break;
            }
        }
        wechatvoiceexport_fs_free_strings(subdirs, subdir_count);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            wechatvoiceexport_fs_free_strings(accounts, account_count);
            return status;
        }
    }

    for (size_t i = 0; i < account_count; ++i) {
        status = wechatvoiceexport_asset_scan_account(out_list, accounts[i]);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
    }
    wechatvoiceexport_fs_free_strings(accounts, account_count);

    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        wechatvoiceexport_assets_free(out_list);
        return status;
    }
    if (out_list->count > 1) {
        qsort(out_list->items, out_list->count, sizeof(*out_list->items),
              wechatvoiceexport_asset_compare);
    }
    return WECHATVOICEEXPORT_STATUS_OK;
}

void wechatvoiceexport_assets_free(wechatvoiceexport_asset_list_t *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i].path);
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

wechatvoiceexport_status_t wechatvoiceexport_asset_find(const wechatvoiceexport_asset_list_t *list,
                                                        const char *selector, size_t *out_index) {
    bool all_digits = true;
    size_t selector_length;

    if (list == NULL || selector == NULL || out_index == NULL || selector[0] == '\0') {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    selector_length = strlen(selector);
    for (size_t i = 0; i < selector_length; ++i) {
        if (selector[i] < '0' || selector[i] > '9') {
            all_digits = false;
            break;
        }
    }

    if (all_digits) {
        size_t value = 0;
        for (size_t i = 0; i < selector_length; ++i) {
            size_t digit = (size_t)(selector[i] - '0');
            if (value > (SIZE_MAX - digit) / 10) {
                return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
            }
            value = value * 10 + digit;
        }
        if (value == 0 || value > list->count) {
            return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
        }
        *out_index = value - 1;
        return WECHATVOICEEXPORT_STATUS_OK;
    }

    if (selector_length > 32 || !wechatvoiceexport_asset_is_hex(selector, selector_length)) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }

    {
        size_t matches = 0;
        size_t match_index = 0;
        for (size_t i = 0; i < list->count; ++i) {
            bool prefix = true;
            for (size_t j = 0; j < selector_length; ++j) {
                char left = list->items[i].md5_hex[j];
                char right = selector[j];
                if (right >= 'A' && right <= 'F') {
                    right = (char)(right - 'A' + 'a');
                }
                if (left != right) {
                    prefix = false;
                    break;
                }
            }
            if (prefix) {
                ++matches;
                match_index = i;
            }
        }
        if (matches == 0) {
            return WECHATVOICEEXPORT_STATUS_NOT_FOUND;
        }
        if (matches > 1) {
            return WECHATVOICEEXPORT_STATUS_AMBIGUOUS;
        }
        *out_index = match_index;
        return WECHATVOICEEXPORT_STATUS_OK;
    }
}

bool wechatvoiceexport_asset_expected_plain_range(uint64_t ciphertext_size, uint64_t *out_min,
                                                  uint64_t *out_max) {
    uint64_t minimum;

    if (out_min == NULL || out_max == NULL || ciphertext_size < 17) {
        return false;
    }
    minimum = ciphertext_size - 16;
    if (minimum < 10) {
        minimum = 10;
    }
    *out_min = minimum;
    *out_max = ciphertext_size - 1;
    return true;
}
