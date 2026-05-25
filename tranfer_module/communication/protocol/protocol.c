#include "protocol.h"
#include "../utils/crc.h"
#include <string.h>

static void protocol_reset(comm_protocol_t *ctx)
{
    ctx->state         = PROTOCOL_STATE_WAIT_START;
    ctx->unpack_crc    = 0xFFFFu;
    ctx->unpack_offset = 0;
    ctx->unpack_length = 0;
}

static int32_t protocol_pack(comm_protocol_t *ctx,
                              uint8_t *output, uint16_t output_size,
                              const uint8_t *input, uint16_t input_len)
{
    if (input_len > PROTOCOL_MAX_PAYLOAD) {
        if (ctx->on_pack_fail) {
            ctx->on_pack_fail(ctx, output, output_size, PROTOCOL_EVENT_OVERFLOW);
        }
        return -1;
    }

    uint16_t frame_len = (uint16_t)(input_len + PROTOCOL_FRAME_OVERHEAD);
    if (frame_len > output_size) {
        if (ctx->on_pack_fail) {
            ctx->on_pack_fail(ctx, output, output_size, PROTOCOL_EVENT_OVERFLOW);
        }
        return -1;
    }

    uint16_t idx = 0;
    output[idx++] = PROTOCOL_START_BYTE;
    output[idx++] = (uint8_t)(input_len & 0xFFu);
    output[idx++] = (uint8_t)((input_len >> 8) & 0xFFu);
    memcpy(&output[idx], input, input_len);
    idx += input_len;

    /* CRC covers LENGTH(2) + PAYLOAD(N) */
    uint16_t crc = crc16_mcrf4xx(&output[1], (uint16_t)(2u + input_len));
    output[idx++] = (uint8_t)(crc & 0xFFu);
    output[idx++] = (uint8_t)((crc >> 8) & 0xFFu);
    output[idx++] = PROTOCOL_STOP_BYTE;

    if (ctx->on_pack_done) {
        ctx->on_pack_done(ctx, output, idx);
    }
    return (int32_t)idx;
}

static int32_t protocol_feed(comm_protocol_t *ctx,
                              uint8_t *output, uint16_t output_size,
                              uint8_t byte)
{
    switch (ctx->state)
    {
        case PROTOCOL_STATE_WAIT_START:
            if (byte == PROTOCOL_START_BYTE) {
                ctx->unpack_offset = 0;
                ctx->unpack_length = 0;
                ctx->unpack_crc    = 0xFFFFu;
                ctx->state         = PROTOCOL_STATE_READ_LENGTH;
            }
            return 0;

        case PROTOCOL_STATE_READ_LENGTH:
            ctx->unpack_crc = crc16_mcrf4xx_update(ctx->unpack_crc, byte);
            if (ctx->unpack_offset == 0u) {
                ctx->unpack_length = byte;
                ctx->unpack_offset = 1;
            } else {
                ctx->unpack_length |= (uint16_t)(byte << 8);
                ctx->unpack_offset  = 0;
                if (ctx->unpack_length > PROTOCOL_MAX_PAYLOAD ||
                    (uint16_t)(ctx->unpack_length + PROTOCOL_FRAME_OVERHEAD) > output_size) {
                    ctx->state = PROTOCOL_STATE_WAIT_START;
                    if (ctx->on_parser_fail) {
                        ctx->on_parser_fail(ctx, output, output_size, PROTOCOL_EVENT_OVERFLOW);
                    }
                    return -1;
                }
                ctx->state = (ctx->unpack_length == 0u)
                             ? PROTOCOL_STATE_READ_CRC
                             : PROTOCOL_STATE_READ_PAYLOAD;
            }
            return 0;

        case PROTOCOL_STATE_READ_PAYLOAD:
            output[ctx->unpack_offset] = byte;
            ctx->unpack_crc            = crc16_mcrf4xx_update(ctx->unpack_crc, byte);
            ctx->unpack_offset++;
            if (ctx->unpack_offset >= ctx->unpack_length) {
                ctx->unpack_offset = 0;
                ctx->state         = PROTOCOL_STATE_READ_CRC;
            }
            return 0;

        case PROTOCOL_STATE_READ_CRC:
            /* output[unpack_length] is validated in-bounds (size >= unpack_length + 6).
               Use it as scratch to hold the received CRC low byte. */
            if (ctx->unpack_offset == 0u) {
                output[ctx->unpack_length] = byte;
                ctx->unpack_offset = 1;
            } else {
                uint16_t received_crc = (uint16_t)(output[ctx->unpack_length]) |
                                        (uint16_t)(byte << 8);
                ctx->unpack_offset = 0;
                if (received_crc != ctx->unpack_crc) {
                    ctx->state = PROTOCOL_STATE_WAIT_START;
                    if (ctx->on_parser_fail) {
                        ctx->on_parser_fail(ctx, output, output_size, PROTOCOL_EVENT_CRC_ERROR);
                    }
                    return -1;
                }
                ctx->state = PROTOCOL_STATE_READ_STOP;
            }
            return 0;

        case PROTOCOL_STATE_READ_STOP:
            ctx->state = PROTOCOL_STATE_WAIT_START;
            if (byte != PROTOCOL_STOP_BYTE) {
                if (ctx->on_parser_fail) {
                    ctx->on_parser_fail(ctx, output, output_size, PROTOCOL_EVENT_FORMAT_ERROR);
                }
                return -1;
            }
            if (ctx->on_parser_done) {
                ctx->on_parser_done(ctx, output, ctx->unpack_length);
            }
            return (int32_t)ctx->unpack_length;

        default:
            ctx->state = PROTOCOL_STATE_WAIT_START;
            return -1;
    }
}

void protocol_init(comm_protocol_t *ctx)
{
    ctx->pack           = protocol_pack;
    ctx->feed           = protocol_feed;
    ctx->reset          = protocol_reset;
    ctx->on_parser_fail = NULL;
    ctx->on_parser_done = NULL;
    ctx->on_pack_fail   = NULL;
    ctx->on_pack_done   = NULL;
    ctx->user_data      = NULL;
    protocol_reset(ctx);
}
