#ifndef PACKER_H
#define PACKER_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "data_type.h"

    // Define protocol version

#define PACKER_START_BYTE 0xAC
#define PACKER_END_BYTE 0xBB
#define PACKER_MAX_PAYLOAD_SIZE 1024
#define PACKER_MIN_PAYLOAD_SIZE 1
#define PACKER_CHECKSUM_SIZE 2
#define PACKER_HEADER_SIZE 2 // Start byte +CRC (2 bytes) + END byte
#define PACKER_MAX_MESSAGE_LENTH (PACKER_HEADER_SIZE + PACKER_MAX_PAYLOAD_SIZE + PACKER_CHECKSUM_SIZE)

    enum STATE_PARSE
    {
        WAIT_START,
        READ_LENGTH_HIGH,
        READ_LENGTH_LOW,
        READ_PAYLOAD,
        READ_CRC_HIGH,
        READ_CRC_LOW,
        WAIT_END,
    };

    enum PARSER_ERROR
    {
        PARSER_SUCCESS = 0,
        PARSER_RUNNING,
        PARSER_ERROR_LENGTH_OUT_OF_BOUNDS,
        PARSER_ERROR_CRC_MISMATCH,
        PARSER_ERROR_INVALID_START_BYTE,
        PARSER_ERROR_INVALID_END_BYTE,

    };

    // Function to send data using the protocol
    uint8_t pack_data(const char *data, uint16_t length, uint8_t *buffer_out, uint16_t size_out, uint16_t *packed_length);

    // Function to receive data using the protocol
    uint8_t unpack_data(char *buffer, uint16_t buffer_length, uint8_t *buffer_out, uint16_t size_out);

    uint8_t unpack_data_state(uint8_t byte, uint8_t *buffer_out, uint16_t *offset, uint16_t *length, uint16_t size_out, uint16_t *crc, uint8_t *state);
#ifdef __cplusplus
}
#endif

#endif // PACKER_H