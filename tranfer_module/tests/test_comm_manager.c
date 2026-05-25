#include "utest.h"
#include "../communication/communication_manager.h"
#include <string.h>

/* ---- Mock transport ---- */

static uint8_t       mock_tx_frame[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];
static uint16_t      mock_tx_frame_len;
static int           mock_send_call_count;
static comm_status_t mock_send_return;

static comm_transport_t mock_transport;
static communication_manager_t comm;

static comm_status_t mock_send(const uint8_t *data, uint16_t len)
{
    mock_send_call_count++;
    mock_tx_frame_len = len;
    memcpy(mock_tx_frame, data, len);
    /* Simulate synchronous TX completion. */
    comm_send_done_handler(&comm);
    return mock_send_return;
}

static void mock_transport_reset(void)
{
    memset(&mock_transport, 0, sizeof(mock_transport));
    mock_transport.send  = mock_send;
    /* on_receive and send_done_callback are registered by comm_init. */
    mock_send_call_count = 0;
    mock_tx_frame_len    = 0;
    mock_send_return     = COMM_OK;
}

/* Inject bytes as if they arrived from the transport ISR. */
static void mock_inject_rx(const uint8_t *data, uint16_t len)
{
    comm_on_receive(&comm, (uint8_t *)data, len);
}

/* ---- Event capture ---- */

static comm_event_t captured_events[16];
static uint8_t      captured_data[16][PROTOCOL_MAX_PAYLOAD];
static uint16_t     captured_data_len[16];
static int          event_count;

static void event_cb(comm_event_t event, const uint8_t *data, uint16_t len)
{
    if (event_count < 16) {
        captured_events[event_count] = event;
        if (data && len > 0 && len <= PROTOCOL_MAX_PAYLOAD) {
            memcpy(captured_data[event_count], data, len);
        }
        captured_data_len[event_count] = len;
        event_count++;
    }
}

/* ---- Test fixture ---- */

static comm_protocol_t         proto;
static uint8_t                 tx_queue_buf[2048];
static uint8_t                 rx_queue_buf[2048];

static void setup(void)
{
    mock_transport_reset();
    protocol_init(&proto);

    memset(&comm, 0, sizeof(comm));
    event_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Queue init is done inside comm_init — caller just supplies the buffers.
       Transport callbacks are invoked directly by comm_on_receive()/comm_send_done_handler(). */
    comm_init(&comm, &mock_transport, &proto, event_cb,
              tx_queue_buf, sizeof(tx_queue_buf),
              rx_queue_buf, sizeof(rx_queue_buf));
}

/* ---- Tests ---- */

static void test_init_sets_transport_context(void)
{
    setup();
    UTEST_ASSERT(mock_transport.parent_context == &comm);
    UTEST_PASS();
}

static void test_init_registers_protocol_callbacks(void)
{
    setup();
    UTEST_ASSERT(proto.on_pack_done   != NULL);
    UTEST_ASSERT(proto.on_pack_fail   != NULL);
    UTEST_ASSERT(proto.on_parser_done != NULL);
    UTEST_ASSERT(proto.on_parser_fail != NULL);
    UTEST_PASS();
}

static void test_send_ok(void)
{
    setup();
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    UTEST_ASSERT_EQ(COMM_OK, comm_send(&comm, payload, sizeof(payload)));
    UTEST_PASS();
}

static void test_send_null_returns_invalid_arg(void)
{
    setup();
    UTEST_ASSERT_EQ(COMM_ERR_INVALID_ARG, comm_send(&comm, NULL, 3));
    UTEST_ASSERT_EQ(COMM_ERR_INVALID_ARG, comm_send(&comm, (uint8_t *)"x", 0));
    UTEST_PASS();
}

static void test_process_triggers_transport_send(void)
{
    setup();
    const uint8_t payload[] = {0xAA, 0xBB};
    comm_send(&comm, payload, sizeof(payload));
    comm_process(&comm);
    UTEST_ASSERT_EQ(1, mock_send_call_count);
    UTEST_PASS();
}

static void test_process_fires_tx_done_event(void)
{
    setup();
    const uint8_t payload[] = {0x01};
    comm_send(&comm, payload, sizeof(payload));
    comm_process(&comm);
    /* mock_send fires send_done_callback synchronously → COMM_EVENT_TX_DONE */
    bool got = false;
    for (int i = 0; i < event_count; i++) {
        if (captured_events[i] == COMM_EVENT_TX_DONE) { got = true; break; }
    }
    UTEST_ASSERT(got);
    UTEST_PASS();
}

static void test_on_receive_pushes_to_rx_queue(void)
{
    setup();
    /* Build a valid packed frame and inject it via the transport on_receive callback. */
    const uint8_t payload[] = {0xCA, 0xFE};
    uint8_t frame[32];
    int32_t n = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));

    proto.reset(&proto);
    mock_inject_rx(frame, (uint16_t)n);   /* simulates ISR calling on_receive */
    comm_process(&comm);

    bool got_rx = false;
    for (int i = 0; i < event_count; i++) {
        if (captured_events[i] == COMM_EVENT_RX_DONE) { got_rx = true; break; }
    }
    UTEST_ASSERT(got_rx);
    UTEST_PASS();
}

static void test_loopback_round_trip(void)
{
    setup();
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    comm_send(&comm, payload, sizeof(payload));
    comm_process(&comm);
    /* mock_send captured the frame; inject it back via on_receive (loopback). */
    mock_inject_rx(mock_tx_frame, mock_tx_frame_len);
    comm_process(&comm);

    bool got_rx = false;
    for (int i = 0; i < event_count; i++) {
        if (captured_events[i] == COMM_EVENT_RX_DONE) {
            UTEST_ASSERT_EQ(sizeof(payload), captured_data_len[i]);
            UTEST_ASSERT_MEM(payload, captured_data[i], sizeof(payload));
            got_rx = true;
            break;
        }
    }
    UTEST_ASSERT(got_rx);
    UTEST_PASS();
}

static void test_multiple_frames_queued(void)
{
    setup();
    const uint8_t p1[] = {0x11};
    const uint8_t p2[] = {0x22};
    const uint8_t p3[] = {0x33};

    comm_send(&comm, p1, 1);
    comm_send(&comm, p2, 1);
    comm_send(&comm, p3, 1);

    /* Each process call sends one frame (tx_busy cleared synchronously by mock). */
    comm_process(&comm);
    comm_process(&comm);
    comm_process(&comm);

    UTEST_ASSERT_EQ(3, mock_send_call_count);
    UTEST_PASS();
}

static void test_corrupt_rx_fires_protocol_error(void)
{
    setup();
    const uint8_t payload[] = {0x55};
    uint8_t frame[16];
    int32_t n = proto.pack(&proto, frame, sizeof(frame), payload, sizeof(payload));
    frame[n - 3] ^= 0xFF;  /* corrupt CRC */

    proto.reset(&proto);
    mock_inject_rx(frame, (uint16_t)n);
    comm_process(&comm);

    bool got_err = false;
    for (int i = 0; i < event_count; i++) {
        if (captured_events[i] == COMM_EVENT_PROTOCOL_ERROR) { got_err = true; break; }
    }
    UTEST_ASSERT(got_err);
    UTEST_PASS();
}

static void test_tx_queue_full_returns_error(void)
{
    setup();
    uint8_t payload[100];
    memset(payload, 0xAA, sizeof(payload));
    comm_status_t last = COMM_OK;
    int i = 0;
    while (last == COMM_OK && i++ < 200) {
        last = comm_send(&comm, payload, sizeof(payload));
    }
    UTEST_ASSERT_EQ(COMM_ERR_QUEUE_FULL, last);
    UTEST_PASS();
}

static void test_pack_fail_fires_tx_fail_event(void)
{
    setup();
    /* Payload larger than PROTOCOL_MAX_PAYLOAD forces pack() to fail. */
    uint8_t big[PROTOCOL_MAX_PAYLOAD + 1];
    memset(big, 0, sizeof(big));
    comm_status_t s = comm_send(&comm, big, sizeof(big));
    UTEST_ASSERT_EQ(COMM_ERR_INVALID_ARG, s);

    bool got_fail = false;
    for (int i = 0; i < event_count; i++) {
        if (captured_events[i] == COMM_EVENT_TX_FAIL) { got_fail = true; break; }
    }
    UTEST_ASSERT(got_fail);
    UTEST_PASS();
}

void run_comm_manager_tests(void)
{
    UTEST_SUITE("Communication Manager");
    UTEST_RUN(test_init_sets_transport_context);
    UTEST_RUN(test_init_registers_protocol_callbacks);
    UTEST_RUN(test_send_ok);
    UTEST_RUN(test_send_null_returns_invalid_arg);
    UTEST_RUN(test_process_triggers_transport_send);
    UTEST_RUN(test_process_fires_tx_done_event);
    UTEST_RUN(test_on_receive_pushes_to_rx_queue);
    UTEST_RUN(test_loopback_round_trip);
    UTEST_RUN(test_multiple_frames_queued);
    UTEST_RUN(test_corrupt_rx_fires_protocol_error);
    UTEST_RUN(test_tx_queue_full_returns_error);
    UTEST_RUN(test_pack_fail_fires_tx_fail_event);
}
