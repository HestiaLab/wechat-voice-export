#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Create (or recreate) a scratch directory under TEST_TMP_ROOT. Returns 0 on
 * success and writes the absolute-ish directory path into out_path.
 */
int test_util_make_temp_dir(const char *name, char *out_path, size_t out_size);

/* Remove a directory tree; missing paths are ignored. */
void test_util_remove_tree(const char *path);

/* Write bytes to a file. Returns 0 on success. */
int test_util_write_bytes(const char *path, const void *data, size_t size);

#endif /* TEST_UTIL_H */
