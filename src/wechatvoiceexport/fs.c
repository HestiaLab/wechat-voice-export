#if !defined(_WIN32)
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _POSIX_C_SOURCE 200809L
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _XOPEN_SOURCE 700
#endif

#include "fs.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <windows.h>

#include <direct.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

static bool wechatvoiceexport_fs_is_separator(char value) {
    return value == '/' || value == '\\';
}

char *wechatvoiceexport_fs_strdup(const char *value) {
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

#if defined(_WIN32)
static wchar_t *wechatvoiceexport_fs_utf8_to_wide(const char *value) {
    int length;
    wchar_t *wide;

    if (value == NULL) {
        return NULL;
    }
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide, length) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static char *wechatvoiceexport_fs_wide_to_utf8(const wchar_t *value) {
    int length;
    char *utf8;

    if (value == NULL) {
        return NULL;
    }
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

static wechatvoiceexport_status_t
wechatvoiceexport_fs_join_checked(const char *directory, const char *name, char **out_path) {
    size_t directory_length;
    size_t name_length;
    bool separator_needed;
    char *joined;

    if (directory == NULL || name == NULL || out_path == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    directory_length = strlen(directory);
    name_length = strlen(name);
    if (directory_length == 0) {
        joined = wechatvoiceexport_fs_strdup(name);
        if (joined == NULL) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        *out_path = joined;
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    separator_needed =
        !wechatvoiceexport_fs_is_separator(directory[directory_length - 1]) && name_length > 0;
    joined = (char *)malloc(directory_length + (separator_needed ? 1u : 0u) + name_length + 1);
    if (joined == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    memcpy(joined, directory, directory_length);
    if (separator_needed) {
        joined[directory_length] = '/';
    }
    memcpy(joined + directory_length + (separator_needed ? 1u : 0u), name, name_length + 1);
    *out_path = joined;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t wechatvoiceexport_fs_join(const char *directory, const char *name,
                                                     char **out_path) {
    return wechatvoiceexport_fs_join_checked(directory, name, out_path);
}

wechatvoiceexport_status_t wechatvoiceexport_fs_read_file(const char *path, uint8_t **out_data,
                                                          size_t *out_size) {
    FILE *file = NULL;
    uint8_t *data = NULL;
    size_t size = 0;
    size_t read_size = 0;

    if (path == NULL || out_data == NULL || out_size == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_size = 0;

#if defined(_WIN32)
    {
        wchar_t *wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
        if (wide_path == NULL) {
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
        file = _wfopen(wide_path, L"rb");
        free(wide_path);
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }

#if defined(_WIN32)
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    {
        __int64 length = _ftelli64(file);
        if (length < 0 || (uint64_t)length > (uint64_t)SIZE_MAX) {
            fclose(file);
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        size = (size_t)length;
    }
#else
    if (fseeko(file, 0, SEEK_END) != 0) {
        fclose(file);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    {
        off_t length = ftello(file);
        if (length < 0 || (uint64_t)length > (uint64_t)SIZE_MAX) {
            fclose(file);
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        size = (size_t)length;
    }
#endif
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return WECHATVOICEEXPORT_STATUS_IO;
    }

    data = (uint8_t *)malloc(size == 0 ? 1 : size);
    if (data == NULL) {
        fclose(file);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    if (size > 0) {
        read_size = fread(data, 1, size, file);
    }
    if (read_size != size || fclose(file) != 0) {
        free(data);
        return WECHATVOICEEXPORT_STATUS_IO;
    }

    *out_data = data;
    *out_size = size;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t wechatvoiceexport_fs_write_file(const char *path, const uint8_t *data,
                                                           size_t size) {
    FILE *file = NULL;
    bool failed = false;

    if (path == NULL || (data == NULL && size > 0)) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }

#if defined(_WIN32)
    {
        wchar_t *wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
        if (wide_path == NULL) {
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
        file = _wfopen(wide_path, L"wb");
        if (file == NULL) {
            free(wide_path);
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        if (size > 0 && fwrite(data, 1, size, file) != size) {
            failed = true;
        }
        if (fclose(file) != 0) {
            failed = true;
        }
        if (failed) {
            _wremove(wide_path);
            free(wide_path);
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        free(wide_path);
    }
#else
    file = fopen(path, "wb");
    if (file == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    if (size > 0 && fwrite(data, 1, size, file) != size) {
        failed = true;
    }
    if (fclose(file) != 0) {
        failed = true;
    }
    if (failed) {
        remove(path);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
#endif
    return WECHATVOICEEXPORT_STATUS_OK;
}

bool wechatvoiceexport_fs_file_exists(const char *path) {
    if (path == NULL) {
        return false;
    }
#if defined(_WIN32)
    {
        wchar_t *wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
        DWORD attributes;
        bool exists;
        if (wide_path == NULL) {
            return false;
        }
        attributes = GetFileAttributesW(wide_path);
        exists =
            attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        free(wide_path);
        return exists;
    }
#else
    {
        struct stat info;
        return stat(path, &info) == 0 && S_ISREG(info.st_mode);
    }
#endif
}

bool wechatvoiceexport_fs_dir_exists(const char *path) {
    if (path == NULL) {
        return false;
    }
#if defined(_WIN32)
    {
        wchar_t *wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
        DWORD attributes;
        bool exists;
        if (wide_path == NULL) {
            return false;
        }
        attributes = GetFileAttributesW(wide_path);
        exists =
            attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        free(wide_path);
        return exists;
    }
#else
    {
        struct stat info;
        return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
    }
#endif
}

#if !defined(_WIN32)
static bool wechatvoiceexport_fs_create_dir_one(const char *path) {
    if (wechatvoiceexport_fs_dir_exists(path)) {
        return true;
    }
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}
#endif

#if defined(_WIN32)
wechatvoiceexport_status_t wechatvoiceexport_fs_create_dir(const char *path) {
    wchar_t *wide_path;
    int result;

    if (path == NULL || path[0] == '\0') {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    if (wechatvoiceexport_fs_dir_exists(path)) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
    if (wide_path == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    /* SHCreateDirectoryExW fails with ERROR_PATH_NOT_FOUND on forward slashes
     * when intermediate directories are missing, unlike the other Win32 file
     * APIs used here. */
    for (wchar_t *cursor = wide_path; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'/') {
            *cursor = L'\\';
        }
    }
    result = SHCreateDirectoryExW(NULL, wide_path, NULL);
    free(wide_path);
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    return WECHATVOICEEXPORT_STATUS_IO;
}
#else
wechatvoiceexport_status_t wechatvoiceexport_fs_create_dir(const char *path) {
    char *copy;
    size_t length;
    size_t start = 0;

    if (path == NULL || path[0] == '\0') {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    copy = wechatvoiceexport_fs_strdup(path);
    if (copy == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    length = strlen(copy);
    while (length > 1 && wechatvoiceexport_fs_is_separator(copy[length - 1])) {
        copy[--length] = '\0';
    }

    for (size_t i = start + 1; i < length; ++i) {
        if (wechatvoiceexport_fs_is_separator(copy[i])) {
            char saved = copy[i];
            copy[i] = '\0';
            if (copy[0] != '\0' && !wechatvoiceexport_fs_create_dir_one(copy)) {
                free(copy);
                return WECHATVOICEEXPORT_STATUS_IO;
            }
            copy[i] = saved;
        }
    }
    if (!wechatvoiceexport_fs_create_dir_one(copy)) {
        free(copy);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    free(copy);
    return WECHATVOICEEXPORT_STATUS_OK;
}
#endif

#if defined(_WIN32)
wechatvoiceexport_status_t wechatvoiceexport_fs_walk(const char *directory, bool recursive,
                                                     wechatvoiceexport_fs_visit_fn visit,
                                                     void *context) {
    wchar_t *wide_directory;
    wchar_t *pattern;
    size_t pattern_length;
    WIN32_FIND_DATAW entry;
    HANDLE handle;
    wechatvoiceexport_status_t status = WECHATVOICEEXPORT_STATUS_OK;

    if (directory == NULL || visit == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    wide_directory = wechatvoiceexport_fs_utf8_to_wide(directory);
    if (wide_directory == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    pattern_length = wcslen(wide_directory) + 3;
    pattern = (wchar_t *)malloc(pattern_length * sizeof(wchar_t));
    if (pattern == NULL) {
        free(wide_directory);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    swprintf(pattern, pattern_length, L"%ls\\*", wide_directory);
    free(wide_directory);

    handle = FindFirstFileW(pattern, &entry);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }

    do {
        char *name;
        char *full_path;
        bool is_directory;
        if (wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0) {
            continue;
        }
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        is_directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory && !recursive) {
            continue;
        }
        name = wechatvoiceexport_fs_wide_to_utf8(entry.cFileName);
        if (name == NULL) {
            status = WECHATVOICEEXPORT_STATUS_NOMEM;
            break;
        }
        status = wechatvoiceexport_fs_join(directory, name, &full_path);
        free(name);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
        if (is_directory) {
            status = wechatvoiceexport_fs_walk(full_path, recursive, visit, context);
        } else {
            status = visit(full_path, context);
        }
        free(full_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
    } while (FindNextFileW(handle, &entry));

    if (status == WECHATVOICEEXPORT_STATUS_OK && GetLastError() != ERROR_NO_MORE_FILES) {
        status = WECHATVOICEEXPORT_STATUS_IO;
    }
    FindClose(handle);
    return status;
}
#else
wechatvoiceexport_status_t wechatvoiceexport_fs_walk(const char *directory, bool recursive,
                                                     wechatvoiceexport_fs_visit_fn visit,
                                                     void *context) {
    DIR *dir;
    struct dirent *entry;
    wechatvoiceexport_status_t status = WECHATVOICEEXPORT_STATUS_OK;

    if (directory == NULL || visit == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    dir = opendir(directory);
    if (dir == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *full_path;
        struct stat info;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        status = wechatvoiceexport_fs_join(directory, entry->d_name, &full_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
        if (lstat(full_path, &info) != 0 || S_ISLNK(info.st_mode)) {
            free(full_path);
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            if (recursive) {
                status = wechatvoiceexport_fs_walk(full_path, recursive, visit, context);
            }
        } else if (S_ISREG(info.st_mode)) {
            status = visit(full_path, context);
        }
        free(full_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
    }
    closedir(dir);
    return status;
}
#endif

static int wechatvoiceexport_fs_compare_strings(const void *left, const void *right) {
    const char *const *left_string = (const char *const *)left;
    const char *const *right_string = (const char *const *)right;
    return strcmp(*left_string, *right_string);
}

wechatvoiceexport_status_t wechatvoiceexport_fs_list_subdirs(const char *directory,
                                                             char ***out_dirs, size_t *out_count) {
    char **items = NULL;
    size_t count = 0;

    if (directory == NULL || out_dirs == NULL || out_count == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_dirs = NULL;
    *out_count = 0;

#if defined(_WIN32)
    {
        wchar_t *wide_directory = wechatvoiceexport_fs_utf8_to_wide(directory);
        wchar_t *pattern;
        size_t pattern_length;
        WIN32_FIND_DATAW entry;
        HANDLE handle;

        if (wide_directory == NULL) {
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
        pattern_length = wcslen(wide_directory) + 3;
        pattern = (wchar_t *)malloc(pattern_length * sizeof(wchar_t));
        if (pattern == NULL) {
            free(wide_directory);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        swprintf(pattern, pattern_length, L"%ls\\*", wide_directory);
        free(wide_directory);

        handle = FindFirstFileW(pattern, &entry);
        free(pattern);
        if (handle == INVALID_HANDLE_VALUE) {
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        do {
            char *name;
            char **grown;
            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
                (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0) {
                continue;
            }
            name = wechatvoiceexport_fs_wide_to_utf8(entry.cFileName);
            if (name == NULL) {
                FindClose(handle);
                wechatvoiceexport_fs_free_strings(items, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            grown = (char **)realloc(items, (count + 1) * sizeof(char *));
            if (grown == NULL) {
                free(name);
                FindClose(handle);
                wechatvoiceexport_fs_free_strings(items, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            items = grown;
            items[count++] = name;
        } while (FindNextFileW(handle, &entry));
        FindClose(handle);
    }
#else
    {
        DIR *dir = opendir(directory);
        struct dirent *entry;
        if (dir == NULL) {
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        while ((entry = readdir(dir)) != NULL) {
            char *full_path;
            char *name;
            char **grown;
            struct stat info;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (wechatvoiceexport_fs_join(directory, entry->d_name, &full_path) !=
                WECHATVOICEEXPORT_STATUS_OK) {
                closedir(dir);
                wechatvoiceexport_fs_free_strings(items, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            if (lstat(full_path, &info) != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
                free(full_path);
                continue;
            }
            free(full_path);
            name = wechatvoiceexport_fs_strdup(entry->d_name);
            if (name == NULL) {
                closedir(dir);
                wechatvoiceexport_fs_free_strings(items, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            grown = (char **)realloc(items, (count + 1) * sizeof(char *));
            if (grown == NULL) {
                free(name);
                closedir(dir);
                wechatvoiceexport_fs_free_strings(items, count);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            items = grown;
            items[count++] = name;
        }
        closedir(dir);
    }
#endif

    if (count > 1) {
        qsort(items, count, sizeof(char *), wechatvoiceexport_fs_compare_strings);
    }
    *out_dirs = items;
    *out_count = count;
    return WECHATVOICEEXPORT_STATUS_OK;
}

void wechatvoiceexport_fs_free_strings(char **values, size_t count) {
    if (values == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        free(values[i]);
    }
    free(values);
}

const char *wechatvoiceexport_fs_last_separator(const char *path) {
    const char *last = NULL;
    if (path == NULL) {
        return NULL;
    }
    for (const char *cursor = path; *cursor != '\0'; ++cursor) {
        if (wechatvoiceexport_fs_is_separator(*cursor)) {
            last = cursor;
        }
    }
    return last;
}

#if defined(_WIN32)
static wechatvoiceexport_status_t wechatvoiceexport_fs_absolute(const char *path, char **out_path) {
    wchar_t *wide_path;
    wchar_t *resolved;
    char *result;

    *out_path = NULL;
    wide_path = wechatvoiceexport_fs_utf8_to_wide(path);
    if (wide_path == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    resolved = _wfullpath(NULL, wide_path, 0);
    free(wide_path);
    if (resolved == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    result = wechatvoiceexport_fs_wide_to_utf8(resolved);
    free(resolved);
    if (result == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    *out_path = result;
    return WECHATVOICEEXPORT_STATUS_OK;
}
#else
static wechatvoiceexport_status_t wechatvoiceexport_fs_normalize_posix(const char *absolute,
                                                                       char **out_path) {
    size_t length = strlen(absolute);
    char *copy = (char *)malloc(length + 2);
    char **components;
    size_t component_count = 0;
    size_t capacity = 8;
    size_t total = 1;
    char *result;
    size_t offset = 0;

    if (copy == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    components = (char **)malloc(capacity * sizeof(char *));
    if (components == NULL) {
        free(copy);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    memcpy(copy, absolute, length + 1);

    {
        char *cursor = copy;
        while (*cursor != '\0') {
            char *start;
            while (*cursor == '/') {
                ++cursor;
            }
            if (*cursor == '\0') {
                break;
            }
            start = cursor;
            while (*cursor != '\0' && *cursor != '/') {
                ++cursor;
            }
            if (*cursor != '\0') {
                *cursor++ = '\0';
            }
            if (strcmp(start, ".") == 0) {
                continue;
            }
            if (strcmp(start, "..") == 0) {
                if (component_count > 0) {
                    --component_count;
                }
                continue;
            }
            if (component_count == capacity) {
                char **grown;
                capacity *= 2;
                grown = (char **)realloc(components, capacity * sizeof(char *));
                if (grown == NULL) {
                    free(components);
                    free(copy);
                    return WECHATVOICEEXPORT_STATUS_NOMEM;
                }
                components = grown;
            }
            components[component_count++] = start;
        }
    }

    for (size_t i = 0; i < component_count; ++i) {
        total += strlen(components[i]) + 1;
    }
    result = (char *)malloc(total + 1);
    if (result == NULL) {
        free(components);
        free(copy);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    result[offset++] = '/';
    for (size_t i = 0; i < component_count; ++i) {
        size_t part = strlen(components[i]);
        memcpy(result + offset, components[i], part);
        offset += part;
        if (i + 1 < component_count) {
            result[offset++] = '/';
        }
    }
    result[offset] = '\0';
    free(components);
    free(copy);
    *out_path = result;
    return WECHATVOICEEXPORT_STATUS_OK;
}

static wechatvoiceexport_status_t wechatvoiceexport_fs_absolute(const char *path, char **out_path) {
    char *combined;
    char *cwd;
    size_t cwd_length;
    size_t path_length;
    wechatvoiceexport_status_t status;

    *out_path = NULL;
    if (path[0] == '/') {
        return wechatvoiceexport_fs_normalize_posix(path, out_path);
    }
    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    cwd_length = strlen(cwd);
    path_length = strlen(path);
    combined = (char *)malloc(cwd_length + 1 + path_length + 1);
    if (combined == NULL) {
        free(cwd);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    memcpy(combined, cwd, cwd_length);
    combined[cwd_length] = '/';
    memcpy(combined + cwd_length + 1, path, path_length + 1);
    free(cwd);
    status = wechatvoiceexport_fs_normalize_posix(combined, out_path);
    free(combined);
    return status;
}
#endif

#if defined(_WIN32)
static wechatvoiceexport_status_t wechatvoiceexport_fs_try_final_path(const wchar_t *wide_path,
                                                                      char **out_path) {
    HANDLE handle;
    DWORD needed;
    wchar_t *buffer;
    char *utf8;

    *out_path = NULL;
    handle = CreateFileW(wide_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    needed = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (needed == 0) {
        CloseHandle(handle);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    buffer = (wchar_t *)malloc(((size_t)needed + 1) * sizeof(wchar_t));
    if (buffer == NULL) {
        CloseHandle(handle);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    needed = GetFinalPathNameByHandleW(handle, buffer, needed + 1,
                                       FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (needed == 0) {
        free(buffer);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    if (wcsncmp(buffer, L"\\\\?\\UNC\\", 8) == 0) {
        size_t rest = wcslen(buffer + 8);
        wchar_t *unc = (wchar_t *)malloc((rest + 3) * sizeof(wchar_t));
        if (unc == NULL) {
            free(buffer);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        unc[0] = L'\\';
        unc[1] = L'\\';
        wmemcpy(unc + 2, buffer + 8, rest + 1);
        utf8 = wechatvoiceexport_fs_wide_to_utf8(unc);
        free(unc);
    } else if (wcsncmp(buffer, L"\\\\?\\", 4) == 0) {
        utf8 = wechatvoiceexport_fs_wide_to_utf8(buffer + 4);
    } else {
        utf8 = wechatvoiceexport_fs_wide_to_utf8(buffer);
    }
    free(buffer);
    if (utf8 == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    *out_path = utf8;
    return WECHATVOICEEXPORT_STATUS_OK;
}

static wechatvoiceexport_status_t wechatvoiceexport_fs_canonical_impl(const char *absolute,
                                                                      char **out_path) {
    wchar_t *wide_path = wechatvoiceexport_fs_utf8_to_wide(absolute);
    wchar_t *current;
    char *tail = NULL;
    wechatvoiceexport_status_t status;

    if (wide_path == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    current = _wcsdup(wide_path);
    free(wide_path);
    if (current == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }

    for (int depth = 0; depth < 256; ++depth) {
        char *resolved = NULL;
        status = wechatvoiceexport_fs_try_final_path(current, &resolved);
        if (status == WECHATVOICEEXPORT_STATUS_OK) {
            if (tail != NULL && tail[0] != '\0') {
                char *combined = NULL;
                status = wechatvoiceexport_fs_join_checked(resolved, tail, &combined);
                free(resolved);
                free(current);
                free(tail);
                if (status != WECHATVOICEEXPORT_STATUS_OK) {
                    return status;
                }
                *out_path = combined;
                return WECHATVOICEEXPORT_STATUS_OK;
            }
            free(current);
            free(tail);
            *out_path = resolved;
            return WECHATVOICEEXPORT_STATUS_OK;
        }
        if (status != WECHATVOICEEXPORT_STATUS_IO) {
            free(current);
            free(tail);
            return status;
        }

        {
            wchar_t *separator = wcsrchr(current, L'\\');
            wchar_t *component;
            wchar_t *parent;
            size_t parent_length;
            char *component_utf8;
            char *new_tail;

            if (separator == NULL) {
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_IO;
            }
            component = separator + 1;
            if (*component == '\0') {
                *separator = L'\0';
                continue;
            }
            parent_length = (size_t)(separator - current);
            if (parent_length == 2 && current[1] == L':') {
                parent_length = 3;
            }
            parent =
                (wchar_t *)malloc((parent_length == 0 ? 2 : parent_length + 1) * sizeof(wchar_t));
            if (parent == NULL) {
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            if (parent_length == 0) {
                parent[0] = L'\\';
                parent[1] = L'\0';
            } else {
                wmemcpy(parent, current, parent_length);
                parent[parent_length] = L'\0';
            }
            component_utf8 = wechatvoiceexport_fs_wide_to_utf8(component);
            if (component_utf8 == NULL) {
                free(parent);
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            if (tail == NULL || tail[0] == '\0') {
                new_tail = component_utf8;
            } else {
                size_t component_length = strlen(component_utf8);
                size_t tail_length = strlen(tail);
                new_tail = (char *)malloc(component_length + 1 + tail_length + 1);
                if (new_tail != NULL) {
                    memcpy(new_tail, component_utf8, component_length);
                    new_tail[component_length] = '/';
                    memcpy(new_tail + component_length + 1, tail, tail_length + 1);
                }
                free(component_utf8);
            }
            if (new_tail == NULL) {
                free(parent);
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            free(tail);
            tail = new_tail;
            free(current);
            current = parent;
        }
    }
    free(current);
    free(tail);
    return WECHATVOICEEXPORT_STATUS_IO;
}
#else
static wechatvoiceexport_status_t wechatvoiceexport_fs_canonical_impl(const char *absolute,
                                                                      char **out_path) {
    char *current = wechatvoiceexport_fs_strdup(absolute);
    char *tail = NULL;

    if (current == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }

    for (int depth = 0; depth < 256; ++depth) {
        char *resolved = realpath(current, NULL);
        if (resolved != NULL) {
            if (tail != NULL && tail[0] != '\0') {
                char *combined = NULL;
                wechatvoiceexport_status_t status =
                    wechatvoiceexport_fs_join_checked(resolved, tail, &combined);
                free(resolved);
                free(current);
                free(tail);
                if (status != WECHATVOICEEXPORT_STATUS_OK) {
                    return status;
                }
                *out_path = combined;
                return WECHATVOICEEXPORT_STATUS_OK;
            }
            free(current);
            free(tail);
            *out_path = resolved;
            return WECHATVOICEEXPORT_STATUS_OK;
        }
        if (errno != ENOENT && errno != ENOTDIR) {
            free(current);
            free(tail);
            return WECHATVOICEEXPORT_STATUS_IO;
        }
        {
            char *separator = strrchr(current, '/');
            char *component;
            char *parent;
            char *new_tail;

            if (separator == NULL) {
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_IO;
            }
            component = wechatvoiceexport_fs_strdup(separator + 1);
            if (component == NULL) {
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            if (separator == current) {
                parent = wechatvoiceexport_fs_strdup("/");
            } else {
                *separator = '\0';
                parent = wechatvoiceexport_fs_strdup(current);
            }
            free(current);
            if (parent == NULL) {
                free(component);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            current = parent;
            if (component[0] == '\0') {
                free(component);
                if (strcmp(current, "/") == 0) {
                    free(current);
                    free(tail);
                    return WECHATVOICEEXPORT_STATUS_IO;
                }
                continue;
            }
            if (tail == NULL || tail[0] == '\0') {
                new_tail = component;
            } else {
                size_t component_length = strlen(component);
                size_t tail_length = strlen(tail);
                new_tail = (char *)malloc(component_length + 1 + tail_length + 1);
                if (new_tail != NULL) {
                    memcpy(new_tail, component, component_length);
                    new_tail[component_length] = '/';
                    memcpy(new_tail + component_length + 1, tail, tail_length + 1);
                }
                free(component);
            }
            if (new_tail == NULL) {
                free(current);
                free(tail);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            free(tail);
            tail = new_tail;
            if (strcmp(current, "/") == 0) {
                char *resolved_root = realpath("/", NULL);
                char *combined = NULL;
                if (resolved_root == NULL) {
                    free(current);
                    free(tail);
                    return WECHATVOICEEXPORT_STATUS_IO;
                }
                wechatvoiceexport_status_t status =
                    wechatvoiceexport_fs_join_checked(resolved_root, tail, &combined);
                free(resolved_root);
                free(current);
                free(tail);
                if (status != WECHATVOICEEXPORT_STATUS_OK) {
                    return status;
                }
                *out_path = combined;
                return WECHATVOICEEXPORT_STATUS_OK;
            }
        }
    }
    free(current);
    free(tail);
    return WECHATVOICEEXPORT_STATUS_IO;
}
#endif

wechatvoiceexport_status_t wechatvoiceexport_fs_canonical(const char *path, char **out_path) {
    char *absolute = NULL;
    wechatvoiceexport_status_t status;

    if (path == NULL || out_path == NULL || path[0] == '\0') {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    status = wechatvoiceexport_fs_absolute(path, &absolute);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    status = wechatvoiceexport_fs_canonical_impl(absolute, out_path);
    free(absolute);
    return status;
}

#if defined(_WIN32)
static void wechatvoiceexport_fs_trim_wide_root(wchar_t *path) {
    size_t length = wcslen(path);
    while (length > 1 && path[length - 1] == L'\\') {
        if (length == 3 && path[1] == L':') {
            break;
        }
        path[--length] = L'\0';
    }
}

static bool wechatvoiceexport_fs_wide_equal(const wchar_t *left, const wchar_t *right) {
    return CompareStringOrdinal(left, -1, right, -1, TRUE) == CSTR_EQUAL;
}

static bool wechatvoiceexport_fs_wide_prefix(const wchar_t *value, const wchar_t *prefix,
                                             size_t prefix_length) {
    if (prefix_length > (size_t)INT_MAX) {
        return false;
    }
    return CompareStringOrdinal(value, (int)prefix_length, prefix, (int)prefix_length, TRUE) ==
           CSTR_EQUAL;
}
#else
static void wechatvoiceexport_fs_trim_posix_root(char *path) {
    size_t length = strlen(path);
    while (length > 1 && path[length - 1] == '/') {
        path[--length] = '\0';
    }
}
#endif

wechatvoiceexport_status_t wechatvoiceexport_fs_check_within(const char *child, const char *parent,
                                                             bool *out_within) {
    char *canonical_child = NULL;
    char *canonical_parent = NULL;
    wechatvoiceexport_status_t status;

    if (child == NULL || parent == NULL || out_within == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    *out_within = false;
    status = wechatvoiceexport_fs_canonical(child, &canonical_child);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    status = wechatvoiceexport_fs_canonical(parent, &canonical_parent);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        free(canonical_child);
        return status;
    }

#if defined(_WIN32)
    {
        wchar_t *wide_child = wechatvoiceexport_fs_utf8_to_wide(canonical_child);
        wchar_t *wide_parent = wechatvoiceexport_fs_utf8_to_wide(canonical_parent);
        if (wide_child == NULL || wide_parent == NULL) {
            free(wide_child);
            free(wide_parent);
            free(canonical_child);
            free(canonical_parent);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        for (wchar_t *cursor = wide_child; *cursor != L'\0'; ++cursor) {
            if (*cursor == L'/') {
                *cursor = L'\\';
            }
        }
        for (wchar_t *cursor = wide_parent; *cursor != L'\0'; ++cursor) {
            if (*cursor == L'/') {
                *cursor = L'\\';
            }
        }
        wechatvoiceexport_fs_trim_wide_root(wide_child);
        wechatvoiceexport_fs_trim_wide_root(wide_parent);
        {
            size_t child_length = wcslen(wide_child);
            size_t parent_length = wcslen(wide_parent);
            if (wechatvoiceexport_fs_wide_equal(wide_child, wide_parent)) {
                *out_within = true;
            } else if (parent_length > 0 && child_length >= parent_length &&
                       wechatvoiceexport_fs_wide_prefix(wide_child, wide_parent, parent_length)) {
                *out_within = wide_parent[parent_length - 1] == L'\\' ||
                              wide_child[parent_length] == L'\\' ||
                              wide_child[parent_length] == L'\0';
            }
        }
        free(wide_child);
        free(wide_parent);
    }
#else
    {
        wechatvoiceexport_fs_trim_posix_root(canonical_child);
        wechatvoiceexport_fs_trim_posix_root(canonical_parent);
        {
            size_t child_length = strlen(canonical_child);
            size_t parent_length = strlen(canonical_parent);
            if (strcmp(canonical_child, canonical_parent) == 0) {
                *out_within = true;
            } else if (parent_length > 0 && child_length >= parent_length &&
                       strncmp(canonical_child, canonical_parent, parent_length) == 0) {
                *out_within = canonical_parent[parent_length - 1] == '/' ||
                              canonical_child[parent_length] == '/' ||
                              canonical_child[parent_length] == '\0';
            }
        }
    }
#endif

    free(canonical_child);
    free(canonical_parent);
    return WECHATVOICEEXPORT_STATUS_OK;
}

bool wechatvoiceexport_fs_has_extension(const char *path, const char *extension) {
    size_t path_length;
    size_t extension_length;

    if (path == NULL || extension == NULL) {
        return false;
    }
    path_length = strlen(path);
    extension_length = strlen(extension);
    if (extension_length == 0 || path_length < extension_length) {
        return false;
    }
    for (size_t i = 0; i < extension_length; ++i) {
        char left = path[path_length - extension_length + i];
        char right = extension[i];
        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}
