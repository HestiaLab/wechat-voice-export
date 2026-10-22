#include <wechatvoiceexport/fs.h>
#include <wechatvoiceexport/pipeline.h>

#include "silk_fixture.h"
#include "test_util.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FRAMES 50

static char g_root[600];
static char g_data_root[700];
static char g_output[700];

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(0, test_util_make_temp_dir("pipeline", g_root, sizeof g_root));
    TEST_ASSERT_TRUE(snprintf(g_data_root, sizeof g_data_root, "%s/wechat", g_root) > 0);
    TEST_ASSERT_TRUE(snprintf(g_output, sizeof g_output, "%s/out", g_root) > 0);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_create_dir(g_data_root));
}

void tearDown(void) {
    test_util_remove_tree(g_root);
}

static void test_writes_wav_and_respects_force(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    wechatvoiceexport_export_options_t options;
    wechatvoiceexport_export_result_t result;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    memset(&options, 0, sizeof options);
    options.output_dir = g_output;
    options.base_name = "voice";
    options.data_root = g_data_root;

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    TEST_ASSERT_NOT_NULL(result.wav_path);
    TEST_ASSERT_NULL(result.mp3_path);
    TEST_ASSERT_EQUAL_UINT32(24000u, result.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(24000u, (unsigned)result.sample_count);
    TEST_ASSERT_EQUAL_UINT32(TEST_FRAMES, (unsigned)result.frame_count);
    TEST_ASSERT_TRUE(wechatvoiceexport_fs_file_exists(result.wav_path));
    TEST_ASSERT_NOT_NULL(strstr(result.wav_path, "voice.wav"));
    wechatvoiceexport_export_result_free(&result);
    TEST_ASSERT_NULL(result.wav_path);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));

    options.force = true;
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    TEST_ASSERT_NOT_NULL(result.wav_path);
    wechatvoiceexport_export_result_free(&result);
    free(payload);
}

static void test_rejects_output_inside_data_root(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    char inside[800];
    wechatvoiceexport_export_options_t options;
    wechatvoiceexport_export_result_t result;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(2, 1, &payload, &payload_size));
    TEST_ASSERT_TRUE(snprintf(inside, sizeof inside, "%s/exports", g_data_root) > 0);
    memset(&options, 0, sizeof options);
    options.output_dir = inside;
    options.base_name = "voice";
    options.data_root = g_data_root;

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    TEST_ASSERT_FALSE(wechatvoiceexport_fs_dir_exists(inside));
    TEST_ASSERT_NULL(result.wav_path);
    free(payload);
}

static void test_mp3_failure_keeps_wav(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    wechatvoiceexport_export_options_t options;
    wechatvoiceexport_export_result_t result;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(2, 1, &payload, &payload_size));
    memset(&options, 0, sizeof options);
    options.output_dir = g_output;
    options.base_name = "mp3-failure";
    options.data_root = g_data_root;
    options.write_mp3 = true;
    options.ffmpeg_path = "definitely-not-an-ffmpeg-binary";

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_MP3,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    TEST_ASSERT_NOT_NULL(result.wav_path);
    TEST_ASSERT_NULL(result.mp3_path);
    TEST_ASSERT_TRUE(wechatvoiceexport_fs_file_exists(result.wav_path));
    wechatvoiceexport_export_result_free(&result);
    free(payload);
}

static void test_invalid_arguments_and_decode_failure(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    static const uint8_t junk[16] = {0};
    wechatvoiceexport_export_options_t options;
    wechatvoiceexport_export_result_t result;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(2, 1, &payload, &payload_size));
    memset(&options, 0, sizeof options);
    options.output_dir = g_output;
    options.base_name = "voice";
    options.data_root = g_data_root;

    options.base_name = "a/b";
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    options.base_name = "..";
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));
    options.base_name = "voice";
    options.output_dir = NULL;
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_export_silk(payload, payload_size, &options, &result));

    options.output_dir = g_output;
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_DECODE,
                          wechatvoiceexport_export_silk(junk, sizeof junk, &options, &result));
    free(payload);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_writes_wav_and_respects_force);
    RUN_TEST(test_rejects_output_inside_data_root);
    RUN_TEST(test_mp3_failure_keeps_wav);
    RUN_TEST(test_invalid_arguments_and_decode_failure);
    return UNITY_END();
}
