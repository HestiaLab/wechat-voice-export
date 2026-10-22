#include <wechatvoiceexport/md5.h>

#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void assert_md5(const void *data, size_t size, const char *expected) {
    char hex[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    wechatvoiceexport_md5_hex(data, size, hex);
    TEST_ASSERT_EQUAL_STRING(expected, hex);
}

static void test_rfc1321_vectors(void) {
    assert_md5("", 0, "d41d8cd98f00b204e9800998ecf8427e");
    assert_md5("a", 1, "0cc175b9c0f1b6a831c399e269772661");
    assert_md5("abc", 3, "900150983cd24fb0d6963f7d28e17f72");
    assert_md5("message digest", 14, "f96b697d7cb7938d525a2f31aaf161d0");
    assert_md5("abcdefghijklmnopqrstuvwxyz", 26, "c3fcd3d76192e4007dfb496cca67e13b");
    assert_md5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62,
               "d174ab98d277d9f5a5611c2c9f419d9f");
    assert_md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
               80, "57edf4a22be3c955ac49da2e2107b67a");
}

static void test_streaming_matches_one_shot(void) {
    static const char input[] = "The quick brown fox jumps over the lazy dog";
    char one_shot[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    char streamed[WECHATVOICEEXPORT_MD5_HEX_SIZE];
    wechatvoiceexport_md5_ctx_t ctx;
    uint8_t digest[WECHATVOICEEXPORT_MD5_DIGEST_SIZE];

    wechatvoiceexport_md5_hex(input, sizeof input - 1, one_shot);
    wechatvoiceexport_md5_init(&ctx);
    wechatvoiceexport_md5_update(&ctx, input, 7);
    wechatvoiceexport_md5_update(&ctx, input + 7, 13);
    wechatvoiceexport_md5_update(&ctx, input + 20, sizeof input - 1 - 20);
    wechatvoiceexport_md5_final(&ctx, digest);

    for (size_t i = 0; i < WECHATVOICEEXPORT_MD5_DIGEST_SIZE; ++i) {
        (void)snprintf(streamed + i * 2, 3, "%02x", (unsigned)digest[i]);
    }
    TEST_ASSERT_EQUAL_STRING(one_shot, streamed);
    TEST_ASSERT_EQUAL_STRING("9e107d9d372bb6826bd81d3542a419d6", one_shot);
}

static void test_binary_input_with_embedded_nul(void) {
    static const uint8_t input[8] = {0x00, 0x01, 0x02, 0x00, 0xff, 0xfe, 0x7f, 0x80};
    char hex[WECHATVOICEEXPORT_MD5_HEX_SIZE];

    wechatvoiceexport_md5_hex(input, sizeof input, hex);
    TEST_ASSERT_EQUAL_STRING("4baef6fb59bbe0cb5147e74f48552515", hex);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rfc1321_vectors);
    RUN_TEST(test_streaming_matches_one_shot);
    RUN_TEST(test_binary_input_with_embedded_nul);
    return UNITY_END();
}
