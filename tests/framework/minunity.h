/**
 * @file    minunity.h
 * @brief   Minimal unit test harness with a Unity-compatible macro set.
 *
 * The repository has no external dependencies so that a reviewer can clone and
 * run the whole verification suite with nothing but a C compiler and Python.
 * The assertion macros deliberately carry the names and semantics used by
 * ThrowTheSwitch/Unity, which is the framework used on the target: swapping
 * this header for the real unity.h and adding unity.c to the build is the only
 * change needed to run the very same test cases under Ceedling on the LEON
 * board (see SVP.md section 4).
 */
#ifndef MINUNITY_H
#define MINUNITY_H

#include <stdio.h>
#include <string.h>

static int    mu_tests_run;
static int    mu_tests_failed;
static int    mu_current_failed;
static const char *mu_current_name;

/* Test fixtures, defined by every test file (may be empty). */
void setUp(void);
void tearDown(void);

#define MU_FAIL(fmt, ...)                                                     \
    do {                                                                      \
        mu_current_failed = 1;                                                \
        (void)printf("  FAIL %s:%d: " fmt "\n", __FILE__, __LINE__,           \
                     __VA_ARGS__);                                            \
    } while (0)

#define TEST_ASSERT_TRUE(cond)                                                \
    do { if (!(cond)) { MU_FAIL("expected true: %s", #cond); } } while (0)

#define TEST_ASSERT_FALSE(cond)                                               \
    do { if ((cond)) { MU_FAIL("expected false: %s", #cond); } } while (0)

#define TEST_ASSERT_NULL(ptr)                                                 \
    do { if ((ptr) != NULL) { MU_FAIL("expected NULL: %s", #ptr); } } while (0)

#define TEST_ASSERT_NOT_NULL(ptr)                                             \
    do { if ((ptr) == NULL) { MU_FAIL("unexpected NULL: %s", #ptr); } } while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual)                               \
    do {                                                                      \
        long _e = (long)(expected);                                           \
        long _a = (long)(actual);                                             \
        if (_e != _a) {                                                       \
            MU_FAIL("%s: expected %ld, got %ld", #actual, _e, _a);            \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                              \
    do {                                                                      \
        unsigned long _e = (unsigned long)(expected);                         \
        unsigned long _a = (unsigned long)(actual);                           \
        if (_e != _a) {                                                       \
            MU_FAIL("%s: expected %lu, got %lu", #actual, _e, _a);            \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                             \
    do {                                                                      \
        unsigned long _e = (unsigned long)(expected);                         \
        unsigned long _a = (unsigned long)(actual);                           \
        if (_e != _a) {                                                       \
            MU_FAIL("%s: expected 0x%04lX, got 0x%04lX", #actual, _e, _a);    \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT8(e, a)   TEST_ASSERT_EQUAL_UINT((e), (a))
#define TEST_ASSERT_EQUAL_UINT16(e, a)  TEST_ASSERT_EQUAL_UINT((e), (a))
#define TEST_ASSERT_EQUAL_UINT32(e, a)  TEST_ASSERT_EQUAL_UINT((e), (a))
#define TEST_ASSERT_EQUAL_size_t(e, a)  TEST_ASSERT_EQUAL_UINT((e), (a))
#define TEST_ASSERT_EQUAL(e, a)         TEST_ASSERT_EQUAL_INT((e), (a))

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                       \
    do {                                                                      \
        if (memcmp((expected), (actual), (size_t)(len)) != 0) {               \
            MU_FAIL("%s: buffers differ over %u octets", #actual,             \
                    (unsigned)(len));                                         \
        }                                                                     \
    } while (0)

#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        mu_current_name   = #fn;                                              \
        mu_current_failed = 0;                                                \
        mu_tests_run++;                                                       \
        setUp();                                                              \
        fn();                                                                 \
        tearDown();                                                           \
        if (mu_current_failed != 0) {                                         \
            mu_tests_failed++;                                                \
            (void)printf("[FAIL] %s\n", mu_current_name);                     \
        } else {                                                              \
            (void)printf("[ ok ] %s\n", mu_current_name);                     \
        }                                                                     \
    } while (0)

#define UNITY_BEGIN()                                                         \
    do {                                                                      \
        mu_tests_run    = 0;                                                  \
        mu_tests_failed = 0;                                                  \
        (void)printf("=== %s ===\n", __FILE__);                               \
    } while (0)

#define UNITY_END()                                                           \
    (printf("--- %d test(s), %d failure(s)\n", mu_tests_run, mu_tests_failed), \
     mu_tests_failed)

#endif /* MINUNITY_H */
