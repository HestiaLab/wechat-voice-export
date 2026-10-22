#include <wechatvoiceexport/silk.h>

#include <stdlib.h>
#include <string.h>

#include <SKP_Silk_SDK_API.h>

#define WECHATVOICEEXPORT_SILK_HEADER_PLAIN 9u
#define WECHATVOICEEXPORT_SILK_HEADER_WECHAT 10u
#define WECHATVOICEEXPORT_SILK_MAX_SAMPLES_PER_CALL 576u

static bool wechatvoiceexport_silk_header_size(const uint8_t *data, size_t size,
                                               size_t *out_header_size) {
    if (data == NULL) {
        return false;
    }
    if (size >= WECHATVOICEEXPORT_SILK_HEADER_WECHAT && data[0] == 0x02 &&
        memcmp(data + 1, "#!SILK_V3", 9) == 0) {
        *out_header_size = WECHATVOICEEXPORT_SILK_HEADER_WECHAT;
        return true;
    }
    if (size >= WECHATVOICEEXPORT_SILK_HEADER_PLAIN && memcmp(data, "#!SILK_V3", 9) == 0) {
        *out_header_size = WECHATVOICEEXPORT_SILK_HEADER_PLAIN;
        return true;
    }
    return false;
}

wechatvoiceexport_status_t
wechatvoiceexport_silk_inspect(const uint8_t *data, size_t size,
                               wechatvoiceexport_silk_layout_t *out_layout) {
    size_t header_size = 0;
    size_t position;
    size_t frame_count = 0;

    if (data == NULL || out_layout == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof *out_layout);
    if (!wechatvoiceexport_silk_header_size(data, size, &header_size)) {
        return WECHATVOICEEXPORT_STATUS_DECODE;
    }

    position = header_size;
    while (size - position >= 2) {
        size_t frame_size = (size_t)data[position] | ((size_t)data[position + 1] << 8);
        if (frame_size == 0 || frame_size > size - position - 2) {
            break;
        }
        position += 2 + frame_size;
        ++frame_count;
    }

    if (frame_count == 0) {
        return WECHATVOICEEXPORT_STATUS_DECODE;
    }

    out_layout->header_size = header_size;
    out_layout->frame_count = frame_count;
    out_layout->container_size = position;
    out_layout->complete = position == size;
    return WECHATVOICEEXPORT_STATUS_OK;
}

static wechatvoiceexport_status_t wechatvoiceexport_silk_reserve(int16_t **samples,
                                                                 size_t *capacity, size_t needed) {
    int16_t *grown;
    size_t new_capacity = *capacity;

    if (needed <= *capacity) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }
    if (new_capacity == 0) {
        new_capacity = 1024;
    }
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(int16_t)) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    grown = (int16_t *)realloc(*samples, new_capacity * sizeof(int16_t));
    if (grown == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    *samples = grown;
    *capacity = new_capacity;
    return WECHATVOICEEXPORT_STATUS_OK;
}

wechatvoiceexport_status_t wechatvoiceexport_silk_decode(const uint8_t *data, size_t size,
                                                         uint32_t sample_rate,
                                                         wechatvoiceexport_pcm_t *out_pcm) {
    wechatvoiceexport_silk_layout_t layout;
    SKP_int32 decoder_size = 0;
    void *decoder = NULL;
    SKP_SILK_SDK_DecControlStruct control;
    int16_t *samples = NULL;
    size_t capacity = 0;
    size_t sample_count = 0;
    size_t position;
    wechatvoiceexport_status_t status;

    if (data == NULL || out_pcm == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_pcm, 0, sizeof *out_pcm);
    if (sample_rate != 8000u && sample_rate != 12000u && sample_rate != 16000u &&
        sample_rate != 24000u) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }

    status = wechatvoiceexport_silk_inspect(data, size, &layout);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }
    if (!layout.complete) {
        return WECHATVOICEEXPORT_STATUS_DECODE;
    }

    if (SKP_Silk_SDK_Get_Decoder_Size(&decoder_size) != 0 || decoder_size <= 0) {
        return WECHATVOICEEXPORT_STATUS_DECODE;
    }
    decoder = malloc((size_t)decoder_size);
    if (decoder == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    if (SKP_Silk_SDK_InitDecoder(decoder) != 0) {
        free(decoder);
        return WECHATVOICEEXPORT_STATUS_DECODE;
    }

    memset(&control, 0, sizeof control);
    control.API_sampleRate = (SKP_int32)sample_rate;

    status = wechatvoiceexport_silk_reserve(&samples, &capacity, 1024);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        free(decoder);
        return status;
    }

    position = layout.header_size;
    for (size_t frame = 0; frame < layout.frame_count; ++frame) {
        size_t frame_size = (size_t)data[position] | ((size_t)data[position + 1] << 8);
        const uint8_t *frame_data = data + position + 2;
        SKP_int16 frame_samples[WECHATVOICEEXPORT_SILK_MAX_SAMPLES_PER_CALL];

        position += 2 + frame_size;

        do {
            SKP_int16 produced = 0;
            SKP_int result =
                SKP_Silk_SDK_Decode(decoder, &control, 0, (const SKP_uint8 *)frame_data,
                                    (SKP_int)frame_size, frame_samples, &produced);
            if (result != 0 || produced < 0 ||
                (size_t)produced > WECHATVOICEEXPORT_SILK_MAX_SAMPLES_PER_CALL) {
                free(samples);
                free(decoder);
                return WECHATVOICEEXPORT_STATUS_DECODE;
            }
            status = wechatvoiceexport_silk_reserve(&samples, &capacity,
                                                    sample_count + (size_t)produced);
            if (status != WECHATVOICEEXPORT_STATUS_OK) {
                free(samples);
                free(decoder);
                return status;
            }
            if (produced > 0) {
                memcpy(samples + sample_count, frame_samples, (size_t)produced * sizeof(int16_t));
                sample_count += (size_t)produced;
            }
        } while (control.moreInternalDecoderFrames != 0);
    }

    free(decoder);
    out_pcm->samples = samples;
    out_pcm->sample_count = sample_count;
    out_pcm->sample_rate = sample_rate;
    out_pcm->frame_count = layout.frame_count;
    return WECHATVOICEEXPORT_STATUS_OK;
}

void wechatvoiceexport_pcm_free(wechatvoiceexport_pcm_t *pcm) {
    if (pcm == NULL) {
        return;
    }
    free(pcm->samples);
    memset(pcm, 0, sizeof *pcm);
}
