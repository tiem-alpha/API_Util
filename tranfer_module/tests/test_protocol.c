#include "utest.h"
#include "../communication/protocol/protocol.h"
#include "../communication/utils/crc.h"
#include <string.h>

/* ---- Helpers ---- */

static comm_protocol_t proto;
static uint8_t         out_buf[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];

static void setup(void)
{
    protocol_init(&proto);
    memset(out_buf, 0, sizeof(out_buf));
}

/* Feed an entire byte array through the parser. Returns final feed() result. */
static int32_t feed_all(const uint8_t *data, uint16_t len)
{
    int32_t result = 0;
    for (uint16_t i = 0; i < len; i++) {
        result = proto.feed(&proto, out_buf, (uint16_t)sizeof(out_buf), data[i]);
        if (result < 0) return result;
    }
    return result;
}

/* ---- Pack tests ---- */

static void test_pack_returns_correct_length(void)
{
    setup();
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    int32_t n = proto.pack(&proto, out_buf, sizeof(out_buf), payload, sizeof(payload));
    UTEST_ASSERT_EQ(sizeof(payload) + PROTOCOL_FRAME_OVERHEAD, n);
    UTEST_PASS();
}

static void test_pack_correct_framing(void)
{
    setup();
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    int32_t n = proto.pack(&proto, out_buf, sizeof(out_buf), payload, sizeof(payload));
    (void)n;
    UTEST_ASSERT_EQ(PROTOCOL_START_BYTE, out_buf[0]);
    UTEST_ASSERT_EQ(3, out_buf[1]);   /* length low */
    UTEST_ASSERT_EQ(0, out_buf[2]);   /* length high */
    UTEST_ASSERT_EQ(PROTOCOL_STOP_BYTE, out_buf[n - 1]);
    UTEST_PASS();
}

static void test_pack_crc_in_frame(void)
{
    setup();
    const uint8_t payload[] = {0x10, 0x20};
    int32_t n = proto.pack(&proto, out_buf, sizeof(out_buf), payload, sizeof(payload));

    /* CRC is computed over LENGTH(2) + PAYLOAD */
    uint16_t expected_crc = crc16_mcrf4xx(&out_buf[1], (uint16_t)(2u + sizeof(payload)));
    uint16_t actual_crc   = (uint16_t)(out_buf[n - 3]) | (uint16_t)(out_buf[n - 2] << 8);
    UTEST_ASSERT_EQ(expected_crc, actual_crc);
    UTEST_PASS();
}

static void test_pack_output_too_small_returns_error(void)
{
    setup();
    uint8_t small_buf[4];
    const uint8_t payload[] = {1, 2, 3};
    int32_t n = proto.pack(&proto, small_buf, sizeof(small_buf), payload, sizeof(payload));
    UTEST_ASSERT_EQ(-1, n);
    UTEST_PASS();
}

static void test_pack_zero_payload(void)
{
    setup();
    int32_t n = proto.pack(&proto, out_buf, sizeof(out_buf), (const uint8_t *)"", 0);
    UTEST_ASSERT_EQ(PROTOCOL_FRAME_OVERHEAD, n);
    UTEST_ASSERT_EQ(PROTOCOL_START_BYTE, out_buf[0]);
    UTEST_ASSERT_EQ(PROTOCOL_STOP_BYTE, out_buf[n - 1]);
    UTEST_PASS();
}

/* ---- Feed / parse tests ---- */

static void test_feed_round_trip(void)
{
    setup();
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t frame[32];
    int32_t frame_len = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));

    /* Reset and parse */
    proto.reset(&proto);
    int32_t result = feed_all(frame, (uint16_t)frame_len);
    UTEST_ASSERT_EQ(sizeof(payload), result);
    UTEST_ASSERT_MEM(payload, out_buf, sizeof(payload));
    UTEST_PASS();
}

static void test_feed_zero_payload_round_trip(void)
{
    setup();
    uint8_t frame[16];
    int32_t frame_len = proto.pack(&proto, frame, sizeof(frame), (const uint8_t *)"", 0);
    proto.reset(&proto);
    int32_t result = feed_all(frame, (uint16_t)frame_len);
    UTEST_ASSERT_EQ(0, result);
    UTEST_PASS();
}

static void test_feed_bad_crc_returns_error(void)
{
    setup();
    const uint8_t payload[] = {0x01, 0x02};
    uint8_t frame[16];
    int32_t n = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));

    /* Corrupt CRC bytes */
    frame[n - 3] ^= 0xFF;

    proto.reset(&proto);
    int32_t result = feed_all(frame, (uint16_t)n);
    UTEST_ASSERT_EQ(-1, result);
    UTEST_PASS();
}

static void test_feed_bad_stop_byte_returns_error(void)
{
    setup();
    const uint8_t payload[] = {0x55};
    uint8_t frame[16];
    int32_t n = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));
    frame[n - 1] = 0x00;  /* corrupt STOP */

    proto.reset(&proto);
    int32_t result = feed_all(frame, (uint16_t)n);
    UTEST_ASSERT_EQ(-1, result);
    UTEST_PASS();
}

static void test_feed_ignores_garbage_before_start(void)
{
    setup();
    const uint8_t payload[] = {0xAB};
    uint8_t frame[16];
    int32_t n = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));

    proto.reset(&proto);
    /* Feed junk first */
    const uint8_t junk[] = {0x00, 0xFF, 0x12, 0x34};
    feed_all(junk, sizeof(junk));
    /* Then the real frame */
    int32_t result = feed_all(frame, (uint16_t)n);
    UTEST_ASSERT_EQ(sizeof(payload), result);
    UTEST_ASSERT_EQ(0xAB, out_buf[0]);
    UTEST_PASS();
}

static void test_feed_two_consecutive_frames(void)
{
    setup();
    const uint8_t p1[] = {0x11};
    const uint8_t p2[] = {0x22, 0x33};
    uint8_t f1[16], f2[16];
    int32_t n1 = proto.pack(&proto, f1, sizeof(f1), p1, sizeof(p1));
    int32_t n2 = proto.pack(&proto, f2, sizeof(f2), p2, sizeof(p2));

    proto.reset(&proto);
    int32_t r1 = feed_all(f1, (uint16_t)n1);
    UTEST_ASSERT_EQ(sizeof(p1), r1);
    UTEST_ASSERT_EQ(0x11, out_buf[0]);

    memset(out_buf, 0, sizeof(out_buf));
    int32_t r2 = feed_all(f2, (uint16_t)n2);
    UTEST_ASSERT_EQ(sizeof(p2), r2);
    UTEST_ASSERT_MEM(p2, out_buf, sizeof(p2));
    UTEST_PASS();
}

static void test_feed_max_payload(void)
{
    setup();
    uint8_t big_payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t big_frame[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];
    for (int i = 0; i < PROTOCOL_MAX_PAYLOAD; i++) big_payload[i] = (uint8_t)(i & 0xFF);

    int32_t n = proto.pack(&proto, big_frame, sizeof(big_frame), big_payload, PROTOCOL_MAX_PAYLOAD);
    UTEST_ASSERT_EQ(PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD, n);

    proto.reset(&proto);
    int32_t result = feed_all(big_frame, (uint16_t)n);
    UTEST_ASSERT_EQ(PROTOCOL_MAX_PAYLOAD, result);
    UTEST_ASSERT_MEM(big_payload, out_buf, PROTOCOL_MAX_PAYLOAD);
    UTEST_PASS();
}

/* ---- State machine ---- */

static void test_reset_clears_state(void)
{
    setup();
    /* Feed partial frame then reset */
    proto.feed(&proto, out_buf, sizeof(out_buf), PROTOCOL_START_BYTE);
    UTEST_ASSERT_EQ(PROTOCOL_STATE_READ_LENGTH, proto.state);
    proto.reset(&proto);
    UTEST_ASSERT_EQ(PROTOCOL_STATE_WAIT_START, proto.state);
    UTEST_PASS();
}

void run_protocol_tests(void)
{
    UTEST_SUITE("Protocol");
    UTEST_RUN(test_pack_returns_correct_length);
    UTEST_RUN(test_pack_correct_framing);
    UTEST_RUN(test_pack_crc_in_frame);
    UTEST_RUN(test_pack_output_too_small_returns_error);
    UTEST_RUN(test_pack_zero_payload);
    UTEST_RUN(test_feed_round_trip);
    UTEST_RUN(test_feed_zero_payload_round_trip);
    UTEST_RUN(test_feed_bad_crc_returns_error);
    UTEST_RUN(test_feed_bad_stop_byte_returns_error);
    UTEST_RUN(test_feed_ignores_garbage_before_start);
    UTEST_RUN(test_feed_two_consecutive_frames);
    UTEST_RUN(test_feed_max_payload);
    UTEST_RUN(test_reset_clears_state);
}
