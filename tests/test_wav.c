#include <wechatvoiceexport/fs.h>
#include <wechatvoiceexport/wav.h>

#include "test_util.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_dir[600];

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(0, test_util_make_temp_dir("wav", g_dir, sizeof g_dir));
}

void tearDown(void) {
    test_util_remove_tree(g_dir);
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void test_canonical_header_and_samples(void) {
    static const int16_t samples[5] = {0, 1, -1, 32767, -32768};
    char path[700];
    uint8_t *data = NULL;
    size_t size = 0;

    TEST_ASSERT_TRUE(snprintf(path, sizeof path, "%s/voice.wav", g_dir) > 0);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_wav_write(path, samples, 5, 8000));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_read_file(path, &data, &size));
    TEST_ASSERT_EQUAL_UINT32(54u, (unsigned)size);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", data, 4);
    TEST_ASSERT_EQUAL_UINT32(46u, read_u32(data + 4));
    TEST_ASSERT_EQUAL_MEMORY("WAVE", data + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", data + 12, 4);
    TEST_ASSERT_EQUAL_UINT32(16u, read_u32(data + 16));
    TEST_ASSERT_EQUAL_UINT16(1u, read_u16(data + 20));
    TEST_ASSERT_EQUAL_UINT16(1u, read_u16(data + 22));
    TEST_ASSERT_EQUAL_UINT32(8000u, read_u32(data + 24));
    TEST_ASSERT_EQUAL_UINT32(16000u, read_u32(data + 28));
    TEST_ASSERT_EQUAL_UINT16(2u, read_u16(data + 32));
    TEST_ASSERT_EQUAL_UINT16(16u, read_u16(data + 34));
    TEST_ASSERT_EQUAL_MEMORY("data", data + 36, 4);
    TEST_ASSERT_EQUAL_UINT32(10u, read_u32(data + 40));
    for (size_t i = 0; i < 5; ++i) {
        int16_t value = (int16_t)read_u16(data + 44 + i * 2);
        TEST_ASSERT_EQUAL_INT(samples[i], value);
    }
    free(data);
}

static void test_invalid_arguments(void) {
    char path[700];

    TEST_ASSERT_TRUE(snprintf(path, sizeof path, "%s/empty.wav", g_dir) > 0);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_wav_write(NULL, NULL, 0, 8000));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_wav_write(path, NULL, 1, 8000));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_wav_write(path, NULL, 0, 0));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_wav_write(path, NULL, 0, 16000));
    TEST_ASSERT_TRUE(wechatvoiceexport_fs_file_exists(path));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_canonical_header_and_samples);
    RUN_TEST(test_invalid_arguments);
    return UNITY_END();
}
