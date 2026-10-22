#include <wechatvoiceexport/capture.h>
#include <wechatvoiceexport/md5.h>

#include "silk_fixture.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

#define TEST_FRAMES 50
#define TEST_JUNK_OFFSET 37

void setUp(void) {}

void tearDown(void) {}

static uint8_t *make_image(const uint8_t *payload, size_t payload_size, size_t offset, size_t extra,
                           size_t *out_size) {
    uint8_t *image = (uint8_t *)malloc(payload_size + offset + extra);

    TEST_ASSERT_NOT_NULL(image);
    memset(image, 0x5a, payload_size + offset + extra);
    memcpy(image + offset, payload, payload_size);
    *out_size = payload_size + offset + extra;
    return image;
}

static void test_extract_one_payload_with_dedup(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint8_t *image;
    size_t image_size = 0;
    wechatvoiceexport_candidate_list_t list;
    char expected_md5[WECHATVOICEEXPORT_MD5_HEX_SIZE];

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    image = make_image(payload, payload_size, TEST_JUNK_OFFSET, payload_size + 64, &image_size);
    memset(&list, 0, sizeof list);

    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image, image_size, payload_size, payload_size, &list));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32((unsigned)payload_size, (unsigned)list.items[0].size);
    TEST_ASSERT_EQUAL_UINT32(TEST_FRAMES, (unsigned)list.items[0].frame_count);
    wechatvoiceexport_md5_hex(payload, payload_size, expected_md5);
    TEST_ASSERT_EQUAL_STRING(expected_md5, list.items[0].md5_hex);
    TEST_ASSERT_EQUAL_MEMORY(payload, list.items[0].payload, payload_size);

    /* A second, identical copy in the same image must not add a candidate. */
    memcpy(image + image_size - payload_size, payload, payload_size);
    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image, image_size, payload_size, payload_size, &list));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);

    wechatvoiceexport_capture_candidates_free(&list);
    TEST_ASSERT_NULL(list.items);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)list.count);
    free(image);
    free(payload);
}

static void test_rejects_truncated_and_out_of_range(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint8_t *image;
    size_t image_size = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    image = make_image(payload, payload_size - 1, TEST_JUNK_OFFSET, 0, &image_size);
    memset(&list, 0, sizeof list);

    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image, image_size, payload_size, payload_size, &list));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)list.count);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_capture_extract(image, image_size, payload_size + 1,
                                                            payload_size + 100, &list));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)list.count);

    wechatvoiceexport_capture_candidates_free(&list);
    free(image);
    free(payload);
}

static void test_rejects_header_without_frames(void) {
    static const uint8_t empty_container[16] = {0x02, '#', '!',  'S',  'I',  'L',  'K',  '_',
                                                'V',  '3', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    wechatvoiceexport_candidate_list_t list;

    memset(&list, 0, sizeof list);
    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(empty_container, sizeof empty_container, 10, 16, &list));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)list.count);
    wechatvoiceexport_capture_candidates_free(&list);
}

static void test_nine_byte_header_payload(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint8_t *image;
    size_t image_size = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 0, &payload, &payload_size));
    image = make_image(payload, payload_size, 11, 40, &image_size);
    memset(&list, 0, sizeof list);

    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image, image_size, payload_size, payload_size, &list));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32((unsigned)payload_size, (unsigned)list.items[0].size);
    TEST_ASSERT_EQUAL_UINT8('#', list.items[0].payload[0]);

    wechatvoiceexport_capture_candidates_free(&list);
    free(image);
    free(payload);
}

static void test_merge_across_scans(void) {
    uint8_t *payload_a = NULL;
    uint8_t *payload_b = NULL;
    size_t size_a = 0;
    size_t size_b = 0;
    uint8_t *image_a;
    uint8_t *image_b;
    size_t image_size_a = 0;
    size_t image_size_b = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload_a, &size_a));
    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES + 3, 1, &payload_b, &size_b));
    image_a = make_image(payload_a, size_a, 5, 32, &image_size_a);
    image_b = make_image(payload_b, size_b, 9, 32, &image_size_b);
    memset(&list, 0, sizeof list);

    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image_a, image_size_a, size_a, size_a, &list));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image_b, image_size_b, size_b, size_b, &list));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);

    /* Re-scanning the first image must not duplicate it in the shared list. */
    TEST_ASSERT_EQUAL_INT(
        WECHATVOICEEXPORT_STATUS_OK,
        wechatvoiceexport_capture_extract(image_a, image_size_a, size_a, size_a, &list));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);

    wechatvoiceexport_capture_candidates_free(&list);
    free(image_a);
    free(image_b);
    free(payload_a);
    free(payload_b);
}

static void test_invalid_arguments(void) {
    wechatvoiceexport_candidate_list_t list;
    uint8_t byte = 0;

    memset(&list, 0, sizeof list);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_capture_extract(&byte, 1, 1, 0, &list));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_capture_extract(&byte, 1, 1, 1, NULL));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_capture_extract(NULL, 1, 0, 1, &list));
    wechatvoiceexport_capture_candidates_free(&list);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extract_one_payload_with_dedup);
    RUN_TEST(test_rejects_truncated_and_out_of_range);
    RUN_TEST(test_rejects_header_without_frames);
    RUN_TEST(test_nine_byte_header_payload);
    RUN_TEST(test_merge_across_scans);
    RUN_TEST(test_invalid_arguments);
    return UNITY_END();
}
