#include <wechatvoiceexport/capture.h>

#include <stdlib.h>
#include <string.h>

#include <wechatvoiceexport/silk.h>

#include "md5.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <tlhelp32.h>
#include <wchar.h>
#endif

#define WECHATVOICEEXPORT_CAPTURE_MAX_PAYLOAD ((size_t)64u * 1024u * 1024u)
#define WECHATVOICEEXPORT_CAPTURE_CHUNK ((size_t)4u * 1024u * 1024u)

static bool wechatvoiceexport_capture_has_md5(const wechatvoiceexport_candidate_list_t *list,
                                              const char *md5_hex) {
    for (size_t i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i].md5_hex, md5_hex) == 0) {
            return true;
        }
    }
    return false;
}

static wechatvoiceexport_status_t
wechatvoiceexport_capture_add(wechatvoiceexport_candidate_list_t *list, const uint8_t *payload,
                              size_t size, size_t frame_count) {
    wechatvoiceexport_candidate_t *grown;
    uint8_t *copy;
    char md5_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE];

    wechatvoiceexport_md5_hex(payload, size, md5_hex);
    if (wechatvoiceexport_capture_has_md5(list, md5_hex)) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    copy = (uint8_t *)malloc(size);
    if (copy == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    memcpy(copy, payload, size);
    grown = (wechatvoiceexport_candidate_t *)realloc(list->items,
                                                     (list->count + 1) * sizeof(*list->items));
    if (grown == NULL) {
        free(copy);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    list->items = grown;
    memset(&list->items[list->count], 0, sizeof(list->items[list->count]));
    list->items[list->count].payload = copy;
    list->items[list->count].size = size;
    list->items[list->count].frame_count = frame_count;
    memcpy(list->items[list->count].md5_hex, md5_hex, WECHATVOICEEXPORT_MD5_HEX_SIZE);
    ++list->count;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t
wechatvoiceexport_capture_extract(const uint8_t *buffer, size_t size, uint64_t min_payload,
                                  uint64_t max_payload,
                                  wechatvoiceexport_candidate_list_t *out_list) {
    size_t min_size;
    size_t max_size;
    static const char magic[] = "#!SILK_V3";
    const size_t magic_length = sizeof magic - 1;

    if (out_list == NULL || (buffer == NULL && size > 0) || min_payload > max_payload) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    min_size = min_payload > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)min_payload;
    max_size = max_payload > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)max_payload;
    if (size == 0 || min_size > size) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }

    for (size_t i = 0; i + magic_length <= size; ++i) {
        size_t start;
        size_t limit;
        wechatvoiceexport_silk_layout_t layout;

        if (memcmp(buffer + i, magic, magic_length) != 0) {
            continue;
        }
        start = (i > 0 && buffer[i - 1] == 0x02) ? i - 1 : i;
        {
            size_t available = size - start;
            limit = available < max_size ? available : max_size;
        }
        if (limit < min_size) {
            continue;
        }
        if (wechatvoiceexport_silk_inspect(buffer + start, limit, &layout) !=
            WECHATVOICEEXPORT_STATUS_OK) {
            continue;
        }
        if (layout.container_size < min_size || layout.container_size > max_size) {
            continue;
        }
        {
            wechatvoiceexport_status_t status = wechatvoiceexport_capture_add(
                out_list, buffer + start, layout.container_size, layout.frame_count);
            if (status != WECHATVOICEEXPORT_STATUS_OK) {
                return status;
            }
        }
    }
    return WECHATVOICEEXPORT_STATUS_OK;
}

void wechatvoiceexport_capture_pids_free(wechatvoiceexport_pid_list_t *list) {
    if (list == NULL) {
        return;
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

void wechatvoiceexport_capture_candidates_free(wechatvoiceexport_candidate_list_t *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i].payload);
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

#if defined(_WIN32)
static wchar_t *wechatvoiceexport_capture_utf8_to_wide(const char *value) {
    int length;
    wchar_t *wide;

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

static bool wechatvoiceexport_capture_readable(DWORD protect) {
    if ((protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    switch (protect & 0xffu) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

static wechatvoiceexport_status_t
wechatvoiceexport_capture_scan_region(HANDLE process, uintptr_t base, SIZE_T region_size,
                                      size_t min_payload, size_t max_payload,
                                      wechatvoiceexport_candidate_list_t *out_list) {
    size_t overlap = max_payload + 2;
    size_t buffer_size;
    uint8_t *buffer;
    wechatvoiceexport_status_t status = WECHATVOICEEXPORT_STATUS_OK;

    if (overlap > SIZE_MAX - WECHATVOICEEXPORT_CAPTURE_CHUNK) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    buffer_size = WECHATVOICEEXPORT_CAPTURE_CHUNK + overlap;
    buffer = (uint8_t *)malloc(buffer_size);
    if (buffer == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }

    for (SIZE_T offset = 0; offset < region_size; offset += WECHATVOICEEXPORT_CAPTURE_CHUNK) {
        SIZE_T remaining = region_size - offset;
        SIZE_T wanted = remaining < (SIZE_T)buffer_size ? remaining : (SIZE_T)buffer_size;
        SIZE_T read_size = 0;
        BOOL ok;

        /* ReadProcessMemory takes an address, not an integer offset. */
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        ok = ReadProcessMemory(process, (LPCVOID)(base + (uintptr_t)offset), buffer, wanted,
                               &read_size);
        if (!ok && read_size == 0) {
            continue;
        }
        if ((size_t)read_size < min_payload) {
            continue;
        }
        status = wechatvoiceexport_capture_extract(buffer, (size_t)read_size, min_payload,
                                                   max_payload, out_list);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            break;
        }
    }
    free(buffer);
    return status;
}

wechatvoiceexport_status_t
wechatvoiceexport_capture_list_processes(const char *image_name,
                                         wechatvoiceexport_pid_list_t *out_list) {
    HANDLE snapshot;
    PROCESSENTRY32W entry;
    wchar_t *wide_name;
    uint32_t *items = NULL;
    size_t count = 0;

    if (image_name == NULL || image_name[0] == '\0' || out_list == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_list, 0, sizeof *out_list);
    wide_name = wechatvoiceexport_capture_utf8_to_wide(image_name);
    if (wide_name == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        free(wide_name);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    memset(&entry, 0, sizeof entry);
    entry.dwSize = sizeof entry;
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        free(wide_name);
        return WECHATVOICEEXPORT_STATUS_IO;
    }
    do {
        if (_wcsicmp(entry.szExeFile, wide_name) == 0) {
            uint32_t *grown = (uint32_t *)realloc(items, (count + 1) * sizeof(*items));
            if (grown == NULL) {
                free(items);
                CloseHandle(snapshot);
                free(wide_name);
                return WECHATVOICEEXPORT_STATUS_NOMEM;
            }
            items = grown;
            items[count++] = (uint32_t)entry.th32ProcessID;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    free(wide_name);
    out_list->items = items;
    out_list->count = count;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t
wechatvoiceexport_capture_scan_process(uint32_t pid, uint64_t min_payload, uint64_t max_payload,
                                       wechatvoiceexport_candidate_list_t *out_list) {
    HANDLE process;
    uintptr_t address = 0;
    size_t min_size;
    size_t max_size;
    wechatvoiceexport_status_t status = WECHATVOICEEXPORT_STATUS_OK;

    if (out_list == NULL || min_payload > max_payload || max_payload > SIZE_MAX) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    if (max_payload > WECHATVOICEEXPORT_CAPTURE_MAX_PAYLOAD) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    min_size = (size_t)min_payload;
    max_size = (size_t)max_payload;

    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)pid);
    if (process == NULL) {
        return WECHATVOICEEXPORT_STATUS_IO;
    }

    for (;;) {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t base;
        SIZE_T region_size;
        uintptr_t next;

        /* VirtualQueryEx consumes a raw address value. */
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        if (VirtualQueryEx(process, (LPCVOID)address, &info, sizeof info) == 0) {
            break;
        }
        base = (uintptr_t)info.BaseAddress;
        region_size = info.RegionSize;
        if (region_size == 0) {
            break;
        }
        if (info.State == MEM_COMMIT && wechatvoiceexport_capture_readable(info.Protect)) {
            status = wechatvoiceexport_capture_scan_region(process, base, region_size, min_size,
                                                           max_size, out_list);
            if (status != WECHATVOICEEXPORT_STATUS_OK) {
                break;
            }
        }
        next = base + (uintptr_t)region_size;
        if (next <= address) {
            break;
        }
        address = next;
    }

    CloseHandle(process);
    return status;
}
#else
wechatvoiceexport_status_t
wechatvoiceexport_capture_list_processes(const char *image_name,
                                         wechatvoiceexport_pid_list_t *out_list) {
    (void)image_name;
    if (out_list == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_list, 0, sizeof *out_list);
    return WECHATVOICEEXPORT_STATUS_UNSUPPORTED;
}

wechatvoiceexport_status_t
wechatvoiceexport_capture_scan_process(uint32_t pid, uint64_t min_payload, uint64_t max_payload,
                                       wechatvoiceexport_candidate_list_t *out_list) {
    (void)pid;
    (void)min_payload;
    (void)max_payload;
    if (out_list == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_list, 0, sizeof *out_list);
    return WECHATVOICEEXPORT_STATUS_UNSUPPORTED;
}
#endif
