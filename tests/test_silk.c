#include <wechatvoiceexport/silk.h>

#include "silk_fixture.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

#define TEST_FRAMES 50
#define TEST_SAMPLES_24K 24000
#define TEST_SAMPLES_8K 8000

void setUp(void) {}

void tearDown(void) {}

static void test_inspect_and_decode_24k(void) {
    uint8_t *data = NULL;
    size_t size = 0;
    wechatvoiceexport_silk_layout_t layout;
    wechatvoiceexport_pcm_t pcm;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &data, &size));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_TRUE(size > 10);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_silk_inspect(data, size, &layout));
    TEST_ASSERT_EQUAL_UINT32(10u, (unsigned)layout.header_size);
    TEST_ASSERT_EQUAL_UINT32(TEST_FRAMES, (unsigned)layout.frame_count);
    TEST_ASSERT_EQUAL_UINT32((unsigned)size, (unsigned)layout.container_size);
    TEST_ASSERT_TRUE(layout.complete);

    memset(&pcm, 0, sizeof pcm);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_silk_decode(data, size, 24000, &pcm));
    TEST_ASSERT_EQUAL_UINT32(TEST_FRAMES, (unsigned)pcm.frame_count);
    TEST_ASSERT_EQUAL_UINT32(24000u, pcm.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(TEST_SAMPLES_24K, (unsigned)pcm.sample_count);
    TEST_ASSERT_NOT_NULL(pcm.samples);

    {
        int peak = 0;
        for (size_t i = 0; i < pcm.sample_count; ++i) {
            int value = pcm.samples[i];
            int magnitude = value < 0 ? -value : value;
            if (magnitude > peak) {
                peak = magnitude;
            }
        }
        TEST_ASSERT_TRUE(peak > 1000);
    }

    wechatvoiceexport_pcm_free(&pcm);
    TEST_ASSERT_NULL(pcm.samples);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)pcm.sample_count);
    free(data);
}

static void test_decode_at_8k(void) {
    uint8_t *data = NULL;
    size_t size = 0;
    wechatvoiceexport_pcm_t pcm;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &data, &size));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_silk_decode(data, size, 8000, &pcm));
    TEST_ASSERT_EQUAL_UINT32(8000u, pcm.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(TEST_SAMPLES_8K, (unsigned)pcm.sample_count);
    wechatvoiceexport_pcm_free(&pcm);
    free(data);
}

static void test_nine_byte_header(void) {
    uint8_t *data = NULL;
    size_t size = 0;
    wechatvoiceexport_silk_layout_t layout;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 0, &data, &size));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_silk_inspect(data, size, &layout));
    TEST_ASSERT_EQUAL_UINT32(9u, (unsigned)layout.header_size);
    TEST_ASSERT_TRUE(layout.complete);
    free(data);
}

static void test_incomplete_and_invalid_input(void) {
    uint8_t *data = NULL;
    size_t size = 0;
    wechatvoiceexport_silk_layout_t layout;
    wechatvoiceexport_pcm_t pcm;
    uint8_t junk[16];

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &data, &size));

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_silk_inspect(data, size - 3, &layout));
    TEST_ASSERT_FALSE(layout.complete);
    TEST_ASSERT_TRUE(layout.container_size < size - 3);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_DECODE,
                          wechatvoiceexport_silk_decode(data, size - 3, 24000, &pcm));

    memset(junk, 0x5a, sizeof junk);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_DECODE,
                          wechatvoiceexport_silk_inspect(junk, sizeof junk, &layout));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_silk_inspect(NULL, 0, &layout));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_silk_decode(data, size, 44100, &pcm));

    free(data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inspect_and_decode_24k);
    RUN_TEST(test_decode_at_8k);
    RUN_TEST(test_nine_byte_header);
    RUN_TEST(test_incomplete_and_invalid_input);
    return UNITY_END();
}
