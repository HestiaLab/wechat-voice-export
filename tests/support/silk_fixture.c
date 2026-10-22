#include "silk_fixture.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "SKP_Silk_SDK_API.h"

#define TEST_SILK_RATE 24000
#define TEST_SILK_FRAME_SAMPLES 480
#define TEST_SILK_MAX_FRAME_BYTES 1024

static int test_silk_reserve(uint8_t **data, size_t *capacity, size_t needed) {
    uint8_t *grown;
    size_t new_capacity;

    if (needed <= *capacity) {
        return 1;
    }
    new_capacity = *capacity == 0 ? 4096 : *capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            return 0;
        }
        new_capacity *= 2;
    }
    grown = (uint8_t *)realloc(*data, new_capacity);
    if (grown == NULL) {
        return 0;
    }
    *data = grown;
    *capacity = new_capacity;
    return 1;
}

int test_silk_generate(size_t frame_count, int with_prefix, uint8_t **out_data, size_t *out_size) {
    static const uint8_t header_with_prefix[10] = {0x02, '#', '!', 'S', 'I',
                                                   'L',  'K', '_', 'V', '3'};
    static const uint8_t header_plain[9] = {'#', '!', 'S', 'I', 'L', 'K', '_', 'V', '3'};
    SKP_int32 encoder_size = 0;
    SKP_SILK_SDK_EncControlStruct control;
    void *encoder = NULL;
    uint8_t *data = NULL;
    size_t capacity = 0;
    size_t used;
    size_t header_size = with_prefix ? sizeof header_with_prefix : sizeof header_plain;
    int result = 1;

    if (frame_count == 0 || out_data == NULL || out_size == NULL) {
        return 1;
    }
    *out_data = NULL;
    *out_size = 0;
    if (SKP_Silk_SDK_Get_Encoder_Size(&encoder_size) != 0 || encoder_size <= 0) {
        return 1;
    }
    encoder = malloc((size_t)encoder_size);
    if (encoder == NULL) {
        return 1;
    }
    memset(&control, 0, sizeof control);
    if (SKP_Silk_SDK_InitEncoder(encoder, &control) != 0) {
        goto cleanup;
    }
    /* InitEncoder fills the control struct with defaults, so configure after. */
    control.API_sampleRate = TEST_SILK_RATE;
    control.maxInternalSampleRate = TEST_SILK_RATE;
    control.packetSize = TEST_SILK_FRAME_SAMPLES;
    control.bitRate = 24000;
    control.packetLossPercentage = 0;
    control.complexity = 2;
    control.useInBandFEC = 0;
    control.useDTX = 0;

    if (!test_silk_reserve(&data, &capacity, header_size)) {
        goto cleanup;
    }
    memcpy(data, with_prefix ? header_with_prefix : header_plain, header_size);
    used = header_size;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        SKP_int16 samples[TEST_SILK_FRAME_SAMPLES];
        SKP_uint8 encoded[TEST_SILK_MAX_FRAME_BYTES];
        SKP_int16 encoded_size = (SKP_int16)sizeof encoded;

        for (size_t i = 0; i < TEST_SILK_FRAME_SAMPLES; ++i) {
            double t = ((double)frame * TEST_SILK_FRAME_SAMPLES + (double)i) / TEST_SILK_RATE;
            samples[i] = (SKP_int16)(8000.0 * sin(2.0 * 3.14159265358979323846 * 440.0 * t));
        }
        if (SKP_Silk_SDK_Encode(encoder, &control, samples, TEST_SILK_FRAME_SAMPLES, encoded,
                                &encoded_size) != 0) {
            goto cleanup;
        }
        if (encoded_size <= 0 || (size_t)encoded_size > TEST_SILK_MAX_FRAME_BYTES) {
            goto cleanup;
        }
        if (!test_silk_reserve(&data, &capacity, used + 2 + (size_t)encoded_size)) {
            goto cleanup;
        }
        data[used] = (uint8_t)((uint16_t)encoded_size & 0xffu);
        data[used + 1] = (uint8_t)((uint16_t)encoded_size >> 8);
        memcpy(data + used + 2, encoded, (size_t)encoded_size);
        used += 2 + (size_t)encoded_size;
    }

    *out_data = data;
    *out_size = used;
    data = NULL;
    result = 0;

cleanup:
    free(encoder);
    free(data);
    return result;
}
