#include "test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wechatvoiceexport/fs.h"

#if defined(_WIN32)
#include <direct.h>
#define TEST_UTIL_RMDIR _rmdir
#else
#include <unistd.h>
#define TEST_UTIL_RMDIR rmdir
#endif

#ifndef TEST_TMP_ROOT
#error "TEST_TMP_ROOT must be defined by the build system"
#endif

static wechatvoiceexport_status_t test_util_remove_file(const char *path, void *context) {
    (void)context;
    return remove(path) == 0 ? WECHATVOICEEXPORT_STATUS_OK : WECHATVOICEEXPORT_STATUS_IO;
}

void test_util_remove_tree(const char *path) {
    char **dirs = NULL;
    size_t count = 0;

    if (path == NULL) {
        return;
    }
    if (!wechatvoiceexport_fs_dir_exists(path)) {
        (void)remove(path);
        return;
    }
    (void)wechatvoiceexport_fs_walk(path, true, test_util_remove_file, NULL);
    if (wechatvoiceexport_fs_list_subdirs(path, &dirs, &count) == WECHATVOICEEXPORT_STATUS_OK) {
        for (size_t i = 0; i < count; ++i) {
            test_util_remove_tree(dirs[i]);
        }
        wechatvoiceexport_fs_free_strings(dirs, count);
    }
    (void)TEST_UTIL_RMDIR(path);
}

int test_util_make_temp_dir(const char *name, char *out_path, size_t out_size) {
    int written;

    if (name == NULL || out_path == NULL || out_size == 0) {
        return 1;
    }
    /* TEST_TMP_ROOT is a string literal supplied by CMake, not an integer. */
    // cppcheck-suppress invalidPrintfArgType_s
    written = snprintf(out_path, out_size, "%s/%s", TEST_TMP_ROOT, name);
    if (written < 0 || (size_t)written >= out_size) {
        return 1;
    }
    test_util_remove_tree(out_path);
    {
        wechatvoiceexport_status_t status = wechatvoiceexport_fs_create_dir(out_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            fprintf(stderr, "test_util: cannot create %s (status %d)\n", out_path, (int)status);
            return 1;
        }
    }
    return 0;
}

int test_util_write_bytes(const char *path, const void *data, size_t size) {
    if (path == NULL || (data == NULL && size > 0)) {
        return 1;
    }
    return wechatvoiceexport_fs_write_file(path, (const uint8_t *)data, size) ==
                   WECHATVOICEEXPORT_STATUS_OK
               ? 0
               : 1;
}
