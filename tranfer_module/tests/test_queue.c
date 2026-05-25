#include "utest.h"
#include "../communication/queue/comm_queue.h"

static uint8_t    buf[8];
static comm_queue_t q;

static void setup(void)
{
    comm_queue_init(&q, buf, sizeof(buf));
}

static void test_queue_init_empty(void)
{
    setup();
    UTEST_ASSERT(comm_queue_is_empty(&q));
    UTEST_ASSERT_EQ(0, comm_queue_count(&q));
    UTEST_PASS();
}

static void test_queue_push_one(void)
{
    setup();
    uint8_t data = 0x42;
    uint16_t written = comm_queue_push(&q, &data, 1);
    UTEST_ASSERT_EQ(1, written);
    UTEST_ASSERT_EQ(1, comm_queue_count(&q));
    UTEST_PASS();
}

static void test_queue_push_pop(void)
{
    setup();
    const uint8_t data[] = {0x01, 0x02, 0x03};
    comm_queue_push(&q, data, sizeof(data));

    uint8_t out;
    for (int i = 0; i < 3; i++) {
        bool ok = comm_queue_pop(&q, &out);
        UTEST_ASSERT(ok);
        UTEST_ASSERT_EQ(data[i], out);
    }
    UTEST_ASSERT(comm_queue_is_empty(&q));
    UTEST_PASS();
}

static void test_queue_pop_empty_returns_false(void)
{
    setup();
    uint8_t out;
    UTEST_ASSERT(!comm_queue_pop(&q, &out));
    UTEST_PASS();
}

static void test_queue_overflow(void)
{
    setup();
    /* buf size = 8, max capacity = 7 (one slot reserved for head/tail distinction) */
    const uint8_t data[8] = {1,2,3,4,5,6,7,8};
    uint16_t written = comm_queue_push(&q, data, sizeof(data));
    UTEST_ASSERT_EQ(7, written);
    UTEST_ASSERT(q.overflow);
    UTEST_PASS();
}

static void test_queue_full_then_pop_then_push(void)
{
    setup();
    const uint8_t fill[7] = {0};
    comm_queue_push(&q, fill, 7);
    UTEST_ASSERT_EQ(7, comm_queue_count(&q));

    uint8_t out;
    comm_queue_pop(&q, &out);
    UTEST_ASSERT_EQ(6, comm_queue_count(&q));

    uint8_t one = 0xFF;
    uint16_t w = comm_queue_push(&q, &one, 1);
    UTEST_ASSERT_EQ(1, w);
    UTEST_PASS();
}

static void test_queue_count_wraps(void)
{
    setup();
    /* Push 5, pop 4, push 5 — head wraps around */
    const uint8_t a[5] = {1,2,3,4,5};
    comm_queue_push(&q, a, 5);
    uint8_t dummy;
    for (int i = 0; i < 4; i++) comm_queue_pop(&q, &dummy);
    comm_queue_push(&q, a, 5);
    UTEST_ASSERT_EQ(6, comm_queue_count(&q));
    UTEST_PASS();
}

void run_queue_tests(void)
{
    UTEST_SUITE("Queue");
    UTEST_RUN(test_queue_init_empty);
    UTEST_RUN(test_queue_push_one);
    UTEST_RUN(test_queue_push_pop);
    UTEST_RUN(test_queue_pop_empty_returns_false);
    UTEST_RUN(test_queue_overflow);
    UTEST_RUN(test_queue_full_then_pop_then_push);
    UTEST_RUN(test_queue_count_wraps);
}
