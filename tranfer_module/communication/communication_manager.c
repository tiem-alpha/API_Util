#include "communication_manager.h"
#include <stddef.h>

/* Single static instance for ISR-context transport callbacks. */


/* ---- Protocol callbacks (registered into protocol struct by comm_init) ---- */

static void comm_on_pack_done(comm_protocol_t *ctx,
                              uint8_t *output, uint16_t output_size)
{
    communication_manager_t *comm = (communication_manager_t *)ctx->user_data;

    /* output_size = total frame bytes written by pack(). */
    uint16_t needed     = (uint16_t)(2u + output_size);
    uint16_t free_space = (uint16_t)(comm->tx_queue.size - 1u
                                     - comm_queue_count(&comm->tx_queue));

    if (free_space < needed) {
        comm->_pack_status = COMM_ERR_QUEUE_FULL;
        if (comm->event_cb) {
            comm->event_cb(COMM_EVENT_QUEUE_FULL, NULL, 0);
        }
        return;
    }

    /* 2-byte little-endian length prefix so comm_process can pop whole frames. */
    uint8_t prefix[2] = {
        (uint8_t)(output_size & 0xFFu),
        (uint8_t)(output_size >> 8)
    };
    comm_queue_push(&comm->tx_queue, prefix, 2u);
    comm_queue_push(&comm->tx_queue, output, output_size);
    comm->_pack_status = COMM_OK;
}

static void comm_on_pack_fail(comm_protocol_t *ctx,
                              uint8_t *output, uint16_t output_size,
                              protocol_event_t event)
{
    communication_manager_t *comm = (communication_manager_t *)ctx->user_data;
    (void)output;
    (void)output_size;
    (void)event;
    comm->_pack_status = COMM_ERR_INVALID_ARG;
    if (comm->event_cb) {
        comm->event_cb(COMM_EVENT_TX_FAIL, NULL, 0);
    }
}

static void comm_on_parser_done(comm_protocol_t *ctx,
                                uint8_t *output, uint16_t output_size)
{
    /* output[0..output_size-1] is the decoded payload. */
    communication_manager_t *comm = (communication_manager_t *)ctx->user_data;
    if (comm->event_cb) {
        comm->event_cb(COMM_EVENT_RX_DONE, output, output_size);
    }
}

static void comm_on_parser_fail(comm_protocol_t *ctx,
                                uint8_t *output, uint16_t output_size,
                                protocol_event_t event)
{
    communication_manager_t *comm = (communication_manager_t *)ctx->user_data;
    (void)output;
    (void)output_size;
    (void)event;
    if (comm->event_cb) {
        comm->event_cb(COMM_EVENT_PROTOCOL_ERROR, NULL, 0);
    }
}

static void comm_push_rx(communication_manager_t *comm,
                  const uint8_t           *data,
                  uint16_t                 len);

/* ---- Transport ISR callbacks (registered into transport struct by comm_init) ---- */

/* Called by the transport when raw bytes arrive from ISR or polling. */
void comm_on_receive(communication_manager_t *comm, uint8_t *data, uint16_t len)
{
    if (comm != NULL) {
        comm_push_rx(comm, data, len);
    }
}

/* Called by the transport when a TX transfer completes. from ISR */
void comm_send_done_handler(communication_manager_t *comm)
{
    if (comm == NULL) return;
    comm->tx_busy = false;
    if (comm->event_cb) {
        comm->event_cb(COMM_EVENT_TX_DONE, NULL, 0);
    }
}

/* ---- Public API ---- */

void comm_init(communication_manager_t *comm,
               comm_transport_t        *transport,
               comm_protocol_t         *protocol,
               comm_event_cb_t          event_cb,
               uint8_t                 *tx_buf, uint16_t tx_size,
               uint8_t                 *rx_buf, uint16_t rx_size)
{
    if (comm == NULL || transport == NULL || protocol == NULL ||
        tx_buf == NULL || tx_size == 0 || rx_buf == NULL || rx_size == 0) {
        return;
    }

    comm->transport    = transport;
    comm->protocol     = protocol;
    comm->event_cb     = event_cb;
    comm->tx_busy      = false;
    comm->_pack_status = COMM_OK;

    /* Queue init is owned by the comm manager. */
    comm_queue_init(&comm->tx_queue, tx_buf, tx_size);
    comm_queue_init(&comm->rx_queue, rx_buf, rx_size);
    /* Protocol init is owned by the comm manager since it registers callbacks into the protocol context. */
    protocol_init(comm->protocol);
    /* Transport callbacks are invoked directly by the transport/ISR.
       The transport may still use parent_context if needed. */
    transport->parent_context = comm;

    /* Register protocol callbacks so the comm manager reacts to pack/parse events. */
    protocol->user_data      = comm;
    protocol->on_pack_done   = comm_on_pack_done;
    protocol->on_pack_fail   = comm_on_pack_fail;
    protocol->on_parser_done = comm_on_parser_done;
    protocol->on_parser_fail = comm_on_parser_fail;

    protocol->reset(protocol);
}

comm_status_t comm_send(communication_manager_t *comm,
                        const uint8_t           *data,
                        uint16_t                 len)
{
    if (comm == NULL || data == NULL || len == 0u) {
        if (comm != NULL) {
            comm->_pack_status = COMM_ERR_INVALID_ARG;
            if (comm->event_cb) {
                comm->event_cb(COMM_EVENT_TX_FAIL, NULL, 0);
            }
        }
        return COMM_ERR_INVALID_ARG;
    }
     

    /* _pack_status is written by on_pack_done or on_pack_fail (called synchronously
       inside pack()) and read here after pack() returns. */
    comm->_pack_status = COMM_OK;

    comm->protocol->pack(comm->protocol,
                         comm->pack_buf, (uint16_t)sizeof(comm->pack_buf),
                         data, len);

    return comm->_pack_status;
}

static void comm_push_rx(communication_manager_t *comm,
                  const uint8_t           *data,
                  uint16_t                 len)
{
    if (comm == NULL || data == NULL) return;

    uint16_t written = comm_queue_push(&comm->rx_queue, data, len);
    if (written < len && comm->event_cb) {
        comm->event_cb(COMM_EVENT_QUEUE_OVERFLOW, NULL, 0);
    }
}

void comm_process(communication_manager_t *comm)
{
    if (comm == NULL) return;

    /* TX: dequeue and send one frame when the transport is idle. */
    if (!comm->tx_busy && !comm_queue_is_empty(&comm->tx_queue)) {
        uint8_t len_lo, len_hi;
        if (!comm_queue_pop(&comm->tx_queue, &len_lo) ||
            !comm_queue_pop(&comm->tx_queue, &len_hi)) {
            return;
        }
        uint16_t frame_len = (uint16_t)(len_lo | ((uint16_t)len_hi << 8));

        for (uint16_t i = 0; i < frame_len; i++) {
            if (!comm_queue_pop(&comm->tx_queue, &comm->pack_buf[i])) {
                break;
            }
        }

        comm->tx_busy = true;
        comm_status_t result = comm->transport->send(comm->pack_buf, frame_len);
        if (result != COMM_OK) {
            comm->tx_busy = false;
            if (comm->event_cb) {
                comm->event_cb(COMM_EVENT_TX_FAIL, NULL, 0);
            }
        }
    }

    /* RX: feed queued bytes through the protocol parser one byte at a time.
       on_parser_done and on_parser_fail callbacks deliver all outcomes. */
    uint8_t byte;
    while (comm_queue_pop(&comm->rx_queue, &byte)) {
        comm->protocol->feed(comm->protocol,
                             comm->rx_buf, (uint16_t)sizeof(comm->rx_buf),
                             byte);
    }
}
