#ifndef TEST_SILK_FIXTURE_H
#define TEST_SILK_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Encode a synthetic tone (a 440 Hz sine at 24 kHz) into a SILK v3 container.
 * When `with_prefix` is nonzero the 10-byte `\x02#!SILK_V3` header is emitted,
 * otherwise the 9-byte `#!SILK_V3` header is used. Returns 0 on success.
 */
int test_silk_generate(size_t frame_count, int with_prefix, uint8_t **out_data, size_t *out_size);

#endif /* TEST_SILK_FIXTURE_H */
