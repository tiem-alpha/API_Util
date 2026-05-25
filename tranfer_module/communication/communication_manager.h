#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "queue/comm_queue.h"
#include "protocol/protocol.h"

typedef struct communication_manager_s communication_manager_t;

/* ---- Status codes ---- */
typedef enum
{
    COMM_OK = 0,
    COMM_ERR_BUSY,
    COMM_ERR_QUEUE_FULL,
    COMM_ERR_INVALID_ARG,
} comm_status_t;

/* ---- Events ---- */
typedef enum
{
    COMM_EVENT_RX_DONE,
    COMM_EVENT_TX_DONE,
    COMM_EVENT_TX_FAIL,
    COMM_EVENT_TX_BUSY,
    COMM_EVENT_PROTOCOL_ERROR,
    COMM_EVENT_QUEUE_OVERFLOW,
    COMM_EVENT_QUEUE_FULL,
} comm_event_t;

/* Single callback for all events.
   data and len carry the payload for COMM_EVENT_RX_DONE and COMM_EVENT_TX_DONE only. */
typedef void (*comm_event_cb_t)(comm_event_t event, const uint8_t *data, uint16_t len);

/* ---- Transport interface ---- */

typedef struct
{
    comm_status_t (*send)(const uint8_t *data, uint16_t len);
    void         *parent_context;  /* Optional pointer for transport's internal use. */
} comm_transport_t;

/* ---- Manager structure ---- */
struct communication_manager_s
{
    comm_transport_t *transport;
    comm_protocol_t  *protocol;
    comm_queue_t      tx_queue;
    comm_queue_t      rx_queue;
    comm_event_cb_t   event_cb;
    volatile bool     tx_busy;

    /* Return channel written by on_pack_done / on_pack_fail before comm_send returns. */
    comm_status_t _pack_status;

    /* Internal scratch buffers — do not access directly. */
    uint8_t pack_buf[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];
    uint8_t rx_buf[PROTOCOL_MAX_PAYLOAD];
};

/* ---- API ---- */

/**
 * Initialize the communication manager.
 *
 * Internally calls comm_queue_init() for both queues using the provided buffers.
 * Exposes comm_on_receive() and comm_send_done_handler() for the transport
 * or ISR code to call directly.
 * Registers all four protocol callbacks (on_pack_done, on_pack_fail,
 * on_parser_done, on_parser_fail) into the protocol struct.
 * Calls protocol->reset() to bring the parser to a clean initial state.
 *
 * Only one instance is supported at a time. A second call replaces the registered
 * instance for ISR-facing transport callbacks.
 */
void comm_init(communication_manager_t *comm,
               comm_transport_t        *transport,
               comm_protocol_t         *protocol,
               comm_event_cb_t          event_cb,
               uint8_t                 *tx_buf, uint16_t tx_size,
               uint8_t                 *rx_buf, uint16_t rx_size);

/**
 * Pack data and enqueue the frame for transmission.
 * Internally calls protocol->pack(); the on_pack_done callback pushes the
 * packed frame (with a 2-byte length prefix) onto the TX queue.
 *
 * Returns COMM_ERR_QUEUE_FULL if the TX queue has no space for the frame.
 * Non-ISR context only.
 */
comm_status_t comm_send(communication_manager_t *comm,
                        const uint8_t           *data,
                        uint16_t                 len);

/**
 * Drive TX and RX processing. Call from main loop or a single RTOS task.
 * TX: dequeues one packed frame and hands it to the transport.
 * RX: feeds queued bytes through the protocol parser byte-by-byte;
 *     on_parser_done / on_parser_fail callbacks deliver results to the app.
 * Non-ISR context only.
 */
void comm_process(communication_manager_t *comm);

void comm_on_receive(communication_manager_t *comm, uint8_t *data, uint16_t len);
void comm_send_done_handler(communication_manager_t *comm);

#endif /* COMMUNICATION_MANAGER_H */
