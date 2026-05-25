#ifndef UTEST_H
#define UTEST_H

/* Minimal unit-test framework for embedded C.
   Variables are defined in utest.c; include this header in every test file. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern int         utest_pass;
extern int         utest_fail;
extern const char *utest_suite;

/* --- Macros --- */

#define UTEST_SUITE(name)  do { utest_suite = (name); printf("\n[%s]\n", (name)); } while (0)

#define UTEST_RUN(fn)      do { printf("  %-40s ", #fn); fn(); } while (0)

#define UTEST_PASS()       do { printf("OK\n"); utest_pass++; } while (0)

/* Use inside a test function to mark that a check succeeded (internal). */
#define _UCHECK_PASS()     utest_pass++

/* Use inside a test function to mark failure and return. */
#define _UCHECK_FAIL(msg)  do { \
    printf("FAIL\n    %s:%d  %s\n", __FILE__, __LINE__, (msg)); \
    utest_fail++; \
    return; \
} while (0)

#define UTEST_ASSERT(cond) do { \
    if (!(cond)) { _UCHECK_FAIL("expected: " #cond); } \
    else         { _UCHECK_PASS(); } \
} while (0)

#define UTEST_ASSERT_EQ(expected, actual) do { \
    long long _e = (long long)(expected); \
    long long _a = (long long)(actual); \
    if (_e != _a) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "expected %lld  got %lld  (" #actual ")", _e, _a); \
        _UCHECK_FAIL(_msg); \
    } else { _UCHECK_PASS(); } \
} while (0)

#define UTEST_ASSERT_NEQ(unexpected, actual) do { \
    long long _u = (long long)(unexpected); \
    long long _a = (long long)(actual); \
    if (_u == _a) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "expected != %lld  (" #actual ")", _u); \
        _UCHECK_FAIL(_msg); \
    } else { _UCHECK_PASS(); } \
} while (0)

#define UTEST_ASSERT_MEM(expected, actual, len) do { \
    if (memcmp((expected), (actual), (size_t)(len)) != 0) { \
        _UCHECK_FAIL("memory mismatch (" #actual ", " #len " bytes)"); \
    } else { _UCHECK_PASS(); } \
} while (0)

/* Print summary and return 0 (all pass) or 1 (any fail). */
#define UTEST_REPORT_AND_EXIT() do { \
    printf("\n=== %d passed, %d failed ===\n", utest_pass, utest_fail); \
    return (utest_fail > 0) ? 1 : 0; \
} while (0)

/* Declare a test-suite entry function to use in test_runner.c */
#define UTEST_DECLARE(fn)  void fn(void)

#endif /* UTEST_H */
