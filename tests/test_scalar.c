/* SETUP: width and value guarantees for <wechatvoiceexport/scalar.h>. */
#include <wechatvoiceexport/scalar.h>

#include "unity.h"

/* Compile-time width guarantees. These fail the build if a typedef drifts to a
 * platform-dependent width (for example, ulong mapped to native
 * `unsigned long`, which is 64-bit on LP64 Linux). */
_Static_assert(sizeof(wechatvoiceexport_uchar_t) == 1, "wechatvoiceexport_uchar_t must be 8-bit");
_Static_assert(sizeof(wechatvoiceexport_ushort_t) == 2,
               "wechatvoiceexport_ushort_t must be 16-bit");
_Static_assert(sizeof(wechatvoiceexport_ulong_t) == 4, "wechatvoiceexport_ulong_t must be 32-bit");
_Static_assert(sizeof(wechatvoiceexport_short_t) == 2, "wechatvoiceexport_short_t must be 16-bit");
_Static_assert(sizeof(wechatvoiceexport_long_t) == 4, "wechatvoiceexport_long_t must be 32-bit");
_Static_assert(sizeof(wechatvoiceexport_int64_t) == 8, "wechatvoiceexport_int64_t must be 64-bit");
_Static_assert(sizeof(wechatvoiceexport_uint64_t) == 8,
               "wechatvoiceexport_uint64_t must be 64-bit");
_Static_assert(sizeof(wechatvoiceexport_float_t) == sizeof(float),
               "wechatvoiceexport_float_t must be float");
_Static_assert(sizeof(wechatvoiceexport_double_t) == sizeof(double),
               "wechatvoiceexport_double_t must be double");

/* Unity calls setUp() before each test and tearDown() after. Both are required
 * even when empty. */
void setUp(void) {}

void tearDown(void) {}

static void test_scalar_max_macros(void) {
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, WECHATVOICEEXPORT_UCHAR_MAX);
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, WECHATVOICEEXPORT_USHORT_MAX);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, WECHATVOICEEXPORT_ULONG_MAX);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, WECHATVOICEEXPORT_UINT64_MAX);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, WECHATVOICEEXPORT_SHORT_MIN);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, WECHATVOICEEXPORT_SHORT_MAX);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, WECHATVOICEEXPORT_LONG_MIN);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, WECHATVOICEEXPORT_LONG_MAX);
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, WECHATVOICEEXPORT_INT64_MIN);
    TEST_ASSERT_EQUAL_INT64(INT64_MAX, WECHATVOICEEXPORT_INT64_MAX);
}

static void test_scalar_bool(void) {
    wechatvoiceexport_bool_t yes = true;
    wechatvoiceexport_bool_t no = false;
    TEST_ASSERT_TRUE(yes);
    TEST_ASSERT_FALSE(no);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scalar_max_macros);
    RUN_TEST(test_scalar_bool);
    return UNITY_END();
}
