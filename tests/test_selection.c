#include "wechatvoiceexport/selection.h"

#include <wechatvoiceexport/md5.h>
#include <wechatvoiceexport/silk.h>

#include "silk_fixture.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

#define TEST_FRAMES 50

void setUp(void) {}

void tearDown(void) {}

static void append_candidate(wechatvoiceexport_candidate_list_t *list, const uint8_t *payload,
                             size_t size) {
    wechatvoiceexport_candidate_t *grown;
    wechatvoiceexport_candidate_t *item;
    wechatvoiceexport_silk_layout_t layout;

    grown =
        (wechatvoiceexport_candidate_t *)realloc(list->items, (list->count + 1) * sizeof *grown);
    TEST_ASSERT_NOT_NULL(grown);
    list->items = grown;
    item = &list->items[list->count];
    memset(item, 0, sizeof *item);
    item->payload = (uint8_t *)malloc(size);
    TEST_ASSERT_NOT_NULL(item->payload);
    memcpy(item->payload, payload, size);
    item->size = size;
    if (wechatvoiceexport_silk_inspect(payload, size, &layout) == WECHATVOICEEXPORT_STATUS_OK) {
        item->frame_count = layout.frame_count;
    }
    wechatvoiceexport_md5_hex(payload, size, item->md5_hex);
    ++list->count;
}

static void test_collapses_equivalent_containers(void) {
    uint8_t *with_prefix = NULL;
    uint8_t *without_prefix = NULL;
    size_t with_prefix_size = 0;
    size_t without_prefix_size = 0;
    size_t removed = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &with_prefix, &with_prefix_size));
    TEST_ASSERT_EQUAL_INT(
        0, test_silk_generate(TEST_FRAMES, 0, &without_prefix, &without_prefix_size));
    memset(&list, 0, sizeof list);
    append_candidate(&list, with_prefix, with_prefix_size);
    append_candidate(&list, without_prefix, without_prefix_size);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_selection_collapse_equivalent(&list, &removed));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)removed);
    TEST_ASSERT_EQUAL_UINT32((unsigned)with_prefix_size, (unsigned)list.items[0].size);

    wechatvoiceexport_capture_candidates_free(&list);
    free(with_prefix);
    free(without_prefix);
}

static void test_keeps_distinct_audio(void) {
    uint8_t *short_payload = NULL;
    uint8_t *long_payload = NULL;
    size_t short_size = 0;
    size_t long_size = 0;
    size_t removed = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &short_payload, &short_size));
    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES + 3, 1, &long_payload, &long_size));
    memset(&list, 0, sizeof list);
    append_candidate(&list, short_payload, short_size);
    append_candidate(&list, long_payload, long_size);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_selection_collapse_equivalent(&list, &removed));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)removed);

    wechatvoiceexport_capture_candidates_free(&list);
    free(short_payload);
    free(long_payload);
}

static void test_keeps_undecodable_candidate(void) {
    static const uint8_t truncated[] = {0x02, '#', '!', 'S', 'I',  'L',
                                        'K',  '_', 'V', '3', 0x10, 0x00};
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    size_t removed = 0;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    memset(&list, 0, sizeof list);
    append_candidate(&list, payload, payload_size);
    append_candidate(&list, truncated, sizeof truncated);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_selection_collapse_equivalent(&list, &removed));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)removed);

    wechatvoiceexport_capture_candidates_free(&list);
    free(payload);
}

static void test_single_candidate_is_noop(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    size_t removed = 1;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    memset(&list, 0, sizeof list);
    append_candidate(&list, payload, payload_size);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_selection_collapse_equivalent(&list, &removed));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)removed);

    wechatvoiceexport_capture_candidates_free(&list);
    free(payload);
}

static void test_candidate_cap_keeps_all(void) {
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    size_t removed = 1;
    wechatvoiceexport_candidate_list_t list;

    TEST_ASSERT_EQUAL_INT(0, test_silk_generate(TEST_FRAMES, 1, &payload, &payload_size));
    memset(&list, 0, sizeof list);
    for (size_t i = 0; i < 33; ++i) {
        append_candidate(&list, payload, payload_size);
    }

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_selection_collapse_equivalent(&list, &removed));
    TEST_ASSERT_EQUAL_UINT32(33u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)removed);

    wechatvoiceexport_capture_candidates_free(&list);
    free(payload);
}

static void test_invalid_argument(void) {
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_selection_collapse_equivalent(NULL, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_collapses_equivalent_containers);
    RUN_TEST(test_keeps_distinct_audio);
    RUN_TEST(test_keeps_undecodable_candidate);
    RUN_TEST(test_single_candidate_is_noop);
    RUN_TEST(test_candidate_cap_keeps_all);
    RUN_TEST(test_invalid_argument);
    return UNITY_END();
}
