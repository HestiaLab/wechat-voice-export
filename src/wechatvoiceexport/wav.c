#include <wechatvoiceexport/wav.h>

#include <stdlib.h>
#include <string.h>

#include "fs.h"

#define WECHATVOICEEXPORT_WAV_HEADER_SIZE 44u

static void wechatvoiceexport_wav_write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)(value & 0xffu);
    target[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void wechatvoiceexport_wav_write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)(value & 0xffu);
    target[1] = (uint8_t)((value >> 8) & 0xffu);
    target[2] = (uint8_t)((value >> 16) & 0xffu);
    target[3] = (uint8_t)((value >> 24) & 0xffu);
}

wechatvoiceexport_status_t wechatvoiceexport_wav_write(const char *path, const int16_t *samples,
                                                       size_t sample_count, uint32_t sample_rate) {
    size_t data_bytes;
    uint8_t *buffer;
    wechatvoiceexport_status_t status;

    if (path == NULL || (samples == NULL && sample_count > 0) || sample_rate == 0) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    if (sample_count > ((size_t)UINT32_MAX - WECHATVOICEEXPORT_WAV_HEADER_SIZE) / 2u ||
        sample_rate > UINT32_MAX / 2u) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    data_bytes = sample_count * 2u;

    buffer = (uint8_t *)malloc(WECHATVOICEEXPORT_WAV_HEADER_SIZE + data_bytes);
    if (buffer == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }

    memcpy(buffer, "RIFF", 4);
    wechatvoiceexport_wav_write_u32(buffer + 4, (uint32_t)(36u + data_bytes));
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
    memcpy(buffer + 8, "WAVE", 4);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
    memcpy(buffer + 12, "fmt ", 4);
    wechatvoiceexport_wav_write_u32(buffer + 16, 16u);
    wechatvoiceexport_wav_write_u16(buffer + 20, 1u);
    wechatvoiceexport_wav_write_u16(buffer + 22, 1u);
    wechatvoiceexport_wav_write_u32(buffer + 24, sample_rate);
    wechatvoiceexport_wav_write_u32(buffer + 28, sample_rate * 2u);
    wechatvoiceexport_wav_write_u16(buffer + 32, 2u);
    wechatvoiceexport_wav_write_u16(buffer + 34, 16u);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
    memcpy(buffer + 36, "data", 4);
    wechatvoiceexport_wav_write_u32(buffer + 40, (uint32_t)data_bytes);

    for (size_t i = 0; i < sample_count; ++i) {
        uint16_t value = (uint16_t)samples[i];
        wechatvoiceexport_wav_write_u16(buffer + WECHATVOICEEXPORT_WAV_HEADER_SIZE + i * 2, value);
    }

    status = wechatvoiceexport_fs_write_file(path, buffer,
                                             WECHATVOICEEXPORT_WAV_HEADER_SIZE + data_bytes);
    free(buffer);
    return status;
}
