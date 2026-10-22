#include <wechatvoiceexport/asset.h>
#include <wechatvoiceexport/fs.h>
#include <wechatvoiceexport/md5.h>

#include "test_util.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_root[600];
static char g_account[700];
static char g_note_dir[800];
static char g_temp_dir[800];
static char g_resources[800];
static uint8_t g_content_a[64];
static uint8_t g_content_b[48];
static uint8_t g_content_tiny[10];

void setUp(void) {
    char *path = NULL;

    TEST_ASSERT_EQUAL_INT(0, test_util_make_temp_dir("asset", g_root, sizeof g_root));
    TEST_ASSERT_TRUE(snprintf(g_account, sizeof g_account, "%s/account_wxid", g_root) > 0);
    TEST_ASSERT_TRUE(snprintf(g_note_dir, sizeof g_note_dir, "%s/business/favorite/temp/NoteCache",
                              g_account) > 0);
    TEST_ASSERT_TRUE(
        snprintf(g_temp_dir, sizeof g_temp_dir, "%s/business/favorite/temp", g_account) > 0);
    TEST_ASSERT_TRUE(snprintf(g_resources, sizeof g_resources,
                              "%s/business/xeditor/XEditorBackup/Resources", g_account) > 0);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK, wechatvoiceexport_fs_create_dir(g_note_dir));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_create_dir(g_resources));

    for (size_t i = 0; i < sizeof g_content_a; ++i) {
        g_content_a[i] = (uint8_t)('A' + (int)(i % 26));
    }
    memset(g_content_b, 'B', sizeof g_content_b);
    memset(g_content_tiny, 'T', sizeof g_content_tiny);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_join(g_note_dir, "a1.silk", &path));
    TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(path, g_content_a, sizeof g_content_a));
    free(path);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_join(g_note_dir, "a2.silk", &path));
    TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(path, g_content_a, sizeof g_content_a));
    free(path);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_join(g_temp_dir, "b.silk_temp", &path));
    TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(path, g_content_b, sizeof g_content_b));
    free(path);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_join(g_note_dir, "tiny.silk", &path));
    TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(path, g_content_tiny, sizeof g_content_tiny));
    free(path);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_fs_join(g_note_dir, "note.txt", &path));
    TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(path, g_content_a, sizeof g_content_a));
    free(path);

    {
        char md5_hex[WECHATVOICEEXPORT_MD5_HEX_SIZE];
        char copy_path[900];
        uint8_t reencrypted[sizeof g_content_a];
        wechatvoiceexport_md5_hex(g_content_a, sizeof g_content_a, md5_hex);
        TEST_ASSERT_TRUE(snprintf(copy_path, sizeof copy_path, "%s/%s", g_resources, md5_hex) > 0);
        memset(reencrypted, 'Z', sizeof reencrypted);
        TEST_ASSERT_EQUAL_INT(0, test_util_write_bytes(copy_path, reencrypted, sizeof reencrypted));
    }
}

void tearDown(void) {
    test_util_remove_tree(g_root);
}

static size_t find_asset(const wechatvoiceexport_asset_list_t *list, const char *md5_hex) {
    for (size_t i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i].md5_hex, md5_hex) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static void test_scan_deduplicates_and_verifies_persistent_copy(void) {
    wechatvoiceexport_asset_list_t list;
    char md5_a[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    char md5_b[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    size_t index_a;
    size_t index_b;

    memset(&list, 0, sizeof list);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_assets_scan(g_root, &list));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.ignored_too_small);

    wechatvoiceexport_md5_hex(g_content_a, sizeof g_content_a, md5_a);
    wechatvoiceexport_md5_hex(g_content_b, sizeof g_content_b, md5_b);
    index_a = find_asset(&list, md5_a);
    index_b = find_asset(&list, md5_b);
    TEST_ASSERT_TRUE(index_a != SIZE_MAX);
    TEST_ASSERT_TRUE(index_b != SIZE_MAX);

    TEST_ASSERT_EQUAL_UINT64(sizeof g_content_a, list.items[index_a].ciphertext_size);
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.items[index_a].source_count);
    TEST_ASSERT_TRUE(list.items[index_a].has_persistent_copy);
    TEST_ASSERT_NOT_NULL(strstr(list.items[index_a].path, "a1.silk"));

    TEST_ASSERT_EQUAL_UINT64(sizeof g_content_b, list.items[index_b].ciphertext_size);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)list.items[index_b].source_count);
    TEST_ASSERT_FALSE(list.items[index_b].has_persistent_copy);

    wechatvoiceexport_assets_free(&list);
    TEST_ASSERT_NULL(list.items);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)list.count);
}

static void test_selector_forms(void) {
    wechatvoiceexport_asset_list_t list;
    size_t index = SIZE_MAX;
    char prefix[9];
    char selector[33];

    memset(&list, 0, sizeof list);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_assets_scan(g_root, &list));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned)list.count);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_asset_find(&list, "1", &index));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)index);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_asset_find(&list, "2", &index));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)index);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_NOT_FOUND,
                          wechatvoiceexport_asset_find(&list, "0", &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_NOT_FOUND,
                          wechatvoiceexport_asset_find(&list, "3", &index));

    memcpy(prefix, list.items[0].md5_hex, 8);
    prefix[8] = '\0';
    for (size_t i = 0; i < 8; ++i) {
        if (prefix[i] >= 'a' && prefix[i] <= 'f') {
            prefix[i] = (char)(prefix[i] - 'a' + 'A');
        }
    }
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_asset_find(&list, prefix, &index));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)index);

    memset(selector, 'f', 32);
    selector[32] = '\0';
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_NOT_FOUND,
                          wechatvoiceexport_asset_find(&list, selector, &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_asset_find(&list, "zz", &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_asset_find(&list, "", &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_asset_find(NULL, "1", &index));

    wechatvoiceexport_assets_free(&list);
}

static void test_ambiguous_prefix(void) {
    wechatvoiceexport_asset_t items[2];
    wechatvoiceexport_asset_list_t list;
    size_t index = SIZE_MAX;

    memset(items, 0, sizeof items);
    memcpy(items[0].md5_hex, "abc12300000000000000000000000000", 32);
    memcpy(items[1].md5_hex, "abc45600000000000000000000000000", 32);
    list.items = items;
    list.count = 2;

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_AMBIGUOUS,
                          wechatvoiceexport_asset_find(&list, "abc", &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_AMBIGUOUS,
                          wechatvoiceexport_asset_find(&list, "ABC", &index));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_asset_find(&list, "abc123", &index));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)index);
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_asset_find(&list, "abc456", &index));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned)index);
}

static void test_expected_plain_range(void) {
    uint64_t min_value = 0;
    uint64_t max_value = 0;

    TEST_ASSERT_TRUE(wechatvoiceexport_asset_expected_plain_range(3216, &min_value, &max_value));
    TEST_ASSERT_EQUAL_UINT64(3200, min_value);
    TEST_ASSERT_EQUAL_UINT64(3215, max_value);

    TEST_ASSERT_TRUE(wechatvoiceexport_asset_expected_plain_range(17, &min_value, &max_value));
    TEST_ASSERT_EQUAL_UINT64(10, min_value);
    TEST_ASSERT_EQUAL_UINT64(16, max_value);

    TEST_ASSERT_TRUE(wechatvoiceexport_asset_expected_plain_range(18, &min_value, &max_value));
    TEST_ASSERT_EQUAL_UINT64(10, min_value);
    TEST_ASSERT_EQUAL_UINT64(17, max_value);

    TEST_ASSERT_FALSE(wechatvoiceexport_asset_expected_plain_range(16, &min_value, &max_value));
    TEST_ASSERT_FALSE(wechatvoiceexport_asset_expected_plain_range(100, NULL, &max_value));
    TEST_ASSERT_FALSE(wechatvoiceexport_asset_expected_plain_range(100, &min_value, NULL));
}

static void test_data_root_resolution(void) {
    char *resolved = NULL;
    char missing[900];

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_OK,
                          wechatvoiceexport_data_root_resolve(g_root, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_STRING(g_root, resolved);
    free(resolved);

    TEST_ASSERT_TRUE(snprintf(missing, sizeof missing, "%s/does-not-exist", g_root) > 0);
    resolved = NULL;
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_NOT_FOUND,
                          wechatvoiceexport_data_root_resolve(missing, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_STRING(missing, resolved);
    free(resolved);

    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_data_root_resolve("", &resolved));
    TEST_ASSERT_EQUAL_INT(WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT,
                          wechatvoiceexport_data_root_resolve(g_root, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scan_deduplicates_and_verifies_persistent_copy);
    RUN_TEST(test_selector_forms);
    RUN_TEST(test_ambiguous_prefix);
    RUN_TEST(test_expected_plain_range);
    RUN_TEST(test_data_root_resolution);
    return UNITY_END();
}
