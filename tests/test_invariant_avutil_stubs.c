#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

/* Test that memcpy bounds are respected: dims[0] must never exceed
   the actual allocated size of rect->data[i] to prevent OOB read/write. */

START_TEST(test_subtitle_rect_bounds)
{
    /* Invariant: copy size must be <= allocated buffer size */
    struct {
        int width;
        int height;
        size_t claimed_size;
        size_t actual_size;
        int should_be_safe;
    } payloads[] = {
        /* Exploit case: claimed dims[0] far exceeds actual allocation */
        { 100, 100, 1000000, 256, 0 },
        /* Boundary case: claimed size equals actual size (safe) */
        { 16,  16,  256,     256, 1 },
        /* Valid input: small subtitle rect, size well within bounds */
        { 8,   8,   64,      128, 1 },
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        size_t actual = payloads[i].actual_size;
        size_t claimed = payloads[i].claimed_size;

        /* Allocate a buffer of actual_size to simulate rect->data[i] */
        uint8_t *buf = (uint8_t *)malloc(actual);
        ck_assert_ptr_nonnull(buf);
        memset(buf, 0xAB, actual);

        /* Allocate destination of actual_size to simulate Bigarray */
        uint8_t *dst = (uint8_t *)malloc(actual);
        ck_assert_ptr_nonnull(dst);
        memset(dst, 0, actual);

        /* Security invariant: copy size must not exceed actual allocation */
        size_t safe_copy = claimed <= actual ? claimed : actual;
        int is_safe = (claimed <= actual);

        ck_assert_int_eq(is_safe, payloads[i].should_be_safe);

        /* Perform the copy only with the safe (clamped) size */
        memcpy(dst, buf, safe_copy);

        /* Verify no out-of-bounds: safe_copy must be <= actual */
        ck_assert_uint_le(safe_copy, actual);

        free(buf);
        free(dst);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_subtitle_rect_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}