#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_START_BYTE     ((uint8_t)0xAC)
#define PROTOCOL_STOP_BYTE      ((uint8_t)0xBB)
#define PROTOCOL_MAX_PAYLOAD    ((uint16_t)512)
#define PROTOCOL_FRAME_OVERHEAD ((uint16_t)6)   /* START(1) + LEN(2) + CRC(2) + STOP(1) */

typedef struct comm_protocol_s comm_protocol_t;

typedef enum
{
    PROTOCOL_STATE_WAIT_START,
    PROTOCOL_STATE_READ_LENGTH,
    PROTOCOL_STATE_READ_PAYLOAD,
    PROTOCOL_STATE_READ_CRC,
    PROTOCOL_STATE_READ_STOP,
} protocol_state_t;

typedef enum
{
    PROTOCOL_EVENT_PACKET_SUCCESS,
    PROTOCOL_EVENT_CRC_ERROR,
    PROTOCOL_EVENT_FORMAT_ERROR,
    PROTOCOL_EVENT_OVERFLOW,
} protocol_event_t;

struct comm_protocol_s
{
    /* Pack input into a framed packet.
       Returns total frame bytes written to output, or -1 on error. */
    int32_t (*pack)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                    const uint8_t *input, uint16_t input_len);

    /* Feed one byte to the parser state machine.
       Returns payload length when a complete frame is received,
       0 while parsing is in progress, or -1 on error. */
    int32_t (*feed)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size, uint8_t byte);

    /* Reset parser state to WAIT_START and clear all counters. */
    void    (*reset)(comm_protocol_t *ctx);

    /* Optional notification callbacks — set to NULL if unused. */
    void    (*on_parser_fail)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                              protocol_event_t event);
    void    (*on_parser_done)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size);
    void    (*on_pack_fail)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                            protocol_event_t event);
    void    (*on_pack_done)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size);

    /* Parser state — managed internally; do not modify. */
    protocol_state_t state;
    uint16_t         unpack_crc;
    uint16_t         unpack_offset;
    uint16_t         unpack_length;

    /* Optional user context pointer passed through to callbacks. */
    void *user_data;
};

/* Populate ctx with the default protocol implementation and reset parser state. */
void protocol_init(comm_protocol_t *ctx);

#endif /* PROTOCOL_H */
