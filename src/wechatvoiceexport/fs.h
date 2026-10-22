#ifndef WECHATVOICEEXPORT_FS_H
#define WECHATVOICEEXPORT_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wechatvoiceexport/error.h>

typedef wechatvoiceexport_status_t (*wechatvoiceexport_fs_visit_fn)(const char *path,
                                                                    void *context);

char *wechatvoiceexport_fs_strdup(const char *value);

wechatvoiceexport_status_t wechatvoiceexport_fs_read_file(const char *path, uint8_t **out_data,
                                                          size_t *out_size);
wechatvoiceexport_status_t wechatvoiceexport_fs_write_file(const char *path, const uint8_t *data,
                                                           size_t size);
bool wechatvoiceexport_fs_file_exists(const char *path);
bool wechatvoiceexport_fs_dir_exists(const char *path);
wechatvoiceexport_status_t wechatvoiceexport_fs_create_dir(const char *path);
wechatvoiceexport_status_t wechatvoiceexport_fs_walk(const char *directory, bool recursive,
                                                     wechatvoiceexport_fs_visit_fn visit,
                                                     void *context);
wechatvoiceexport_status_t wechatvoiceexport_fs_join(const char *directory, const char *name,
                                                     char **out_path);
wechatvoiceexport_status_t wechatvoiceexport_fs_list_subdirs(const char *directory,
                                                             char ***out_dirs, size_t *out_count);
void wechatvoiceexport_fs_free_strings(char **values, size_t count);
const char *wechatvoiceexport_fs_last_separator(const char *path);

/*
 * Absolute, lexically normalized path. Existing symbolic links in the prefix
 * are resolved when possible; missing trailing components are preserved so the
 * result can be used for containment checks before a directory is created.
 */
wechatvoiceexport_status_t wechatvoiceexport_fs_canonical(const char *path, char **out_path);
wechatvoiceexport_status_t wechatvoiceexport_fs_check_within(const char *child, const char *parent,
                                                             bool *out_within);
bool wechatvoiceexport_fs_has_extension(const char *path, const char *extension);

#endif /* WECHATVOICEEXPORT_FS_H */
