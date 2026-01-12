#include "packer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "crc16.h"
#include "log.h"



uint8_t pack_data(const char *data, uint16_t length, uint8_t *buffer_out, uint16_t size_out, uint16_t *packed_length)
{
    memset(buffer_out, 0, size_out);
    uint16_t crc = 0;
    if (length < PACKER_MIN_PAYLOAD_SIZE || length > PACKER_MAX_PAYLOAD_SIZE || length > size_out + PACKER_HEADER_SIZE + PACKER_CHECKSUM_SIZE)
    {
        log_print( "Error: Data length out of bounds.\n");
        return PARSER_ERROR_LENGTH_OUT_OF_BOUNDS; // Error: Data length out of bounds
    }
    crc16_init(&crc);
    uint16_t i = 0;
    buffer_out[i] = PACKER_START_BYTE;
    i++;
    buffer_out[i] = (length >> 8) & 0xFF;
    crc16_byte_cal(&crc, buffer_out[i]);
    i++;
    buffer_out[i] = length & 0xFF;
    crc16_byte_cal(&crc, buffer_out[i]);
    i++;
    for (int data_i = 0; data_i < length; data_i++)
    {
        buffer_out[i] = data[data_i];
        crc16_byte_cal(&crc, buffer_out[i]);
        i++;
    }
    buffer_out[i] = (crc >> 8) & 0xFF; // CRC high byte
    i++;
    buffer_out[i] = crc & 0xFF; // CRC low byte
    i++;
    buffer_out[i] = PACKER_END_BYTE;
    i++;
    *packed_length = i;
    log_print( "Data packed successfully.\n");
    return PARSER_SUCCESS; // Return total length of packed data
}

uint8_t unpack_data(char *buffer, uint16_t buffer_length, uint8_t *buffer_out, uint16_t size_out)
{
    // Simulate receiving data
    uint16_t crc = 0;
    uint16_t i = 0;
    if (buffer_length < PACKER_HEADER_SIZE + PACKER_CHECKSUM_SIZE + PACKER_MIN_PAYLOAD_SIZE)
    {
        log_print( "Error: Buffer length too small.\n");
        return PARSER_ERROR_LENGTH_OUT_OF_BOUNDS; // Error: Buffer length too small
    }

    if (buffer[i] != PACKER_START_BYTE)
    {
        log_print( "Error: Invalid start byte.\n");
        return PARSER_ERROR_INVALID_START_BYTE; // Error: Invalid start byte
    }
    i++;
    uint16_t length = ((uint16_t)buffer[i] << 8);
    crc16_byte_cal(&crc, buffer[i]);
    i++;
    length |= (uint16_t)buffer[i];
    crc16_byte_cal(&crc, buffer[i]);
    i++;
    if (length < PACKER_MIN_PAYLOAD_SIZE || length > PACKER_MAX_PAYLOAD_SIZE || length > size_out)
    {
        log_print( "Error: Payload length out of bounds.\n");
        return PARSER_ERROR_LENGTH_OUT_OF_BOUNDS; // Error: Payload length out of bounds
    }
    for (int data_i = 0; data_i < length; data_i++)
    {
        buffer_out[data_i] = buffer[i];
        crc16_byte_cal(&crc, buffer[i]);
        i++;
    }
    uint16_t received_crc = ((uint16_t)buffer[i] << 8);
    i++;
    received_crc |= (uint16_t)buffer[i];
    i++;
    if (crc != received_crc)
    {
        log_print( "Error: CRC mismatch.\n");
        return PARSER_ERROR_CRC_MISMATCH; // Error: CRC mismatch
    }
    if (buffer[i] != PACKER_END_BYTE)
    {
        log_print( "Error: Invalid end byte.\n");
        return PARSER_ERROR_INVALID_END_BYTE; // Error: Invalid end byte
    }
    log_print( "Data unpacked successfully.\n");
    return PARSER_SUCCESS; // Return number of bytes received
}

uint8_t unpack_data_state(uint8_t byte, uint8_t *buffer_out, uint16_t *offset, uint16_t *length, uint16_t size_out, uint16_t *crc, uint8_t *state)
{
    uint8_t ret = PARSER_RUNNING;
    switch (*state)
    {
    case WAIT_START:
        /* code */
        *offset = 0;
        *length = 0;
        if (byte == PACKER_START_BYTE)
        {
            *state = READ_LENGTH_HIGH;
            crc16_init(crc); // Initialize CRC
        }
        else{
            ret = PARSER_ERROR_INVALID_START_BYTE;
        }
        break;

    case READ_LENGTH_HIGH:
        /* code */
        *length = byte << 8; // Store high byte of length
        *state = READ_LENGTH_LOW;
        crc16_byte_cal(crc, byte); // Update CRC with length high byte
        break;
    case READ_LENGTH_LOW:
        /* code */
        *length += byte;                 // Store low byte of length
        crc16_byte_cal(crc, byte); // Update CRC with length low byte
        if (*length < PACKER_MIN_PAYLOAD_SIZE || *length > PACKER_MAX_PAYLOAD_SIZE || *length > size_out)
        {
            *state = WAIT_START; // Reset state on error
            *offset = 0;
            return PARSER_ERROR_LENGTH_OUT_OF_BOUNDS; // Error: Payload length out of bounds
        }
        *offset = 0; // Reset offset for payload
        *state = READ_PAYLOAD;
        break;
    case READ_PAYLOAD:
        /* code */
        buffer_out[*offset] = byte; // Store payload byte
        (*offset)++;
        crc16_byte_cal(crc, byte); // Update CRC with payload byte

        if (*offset >= *length)
        {
            *state = READ_CRC_HIGH;
        }

        break;
    case READ_CRC_HIGH:
        /* code */
        if (byte != ((*crc >> 8) & 0xFF))
        {
            *state = WAIT_START; // Reset state on error
            return PARSER_ERROR_CRC_MISMATCH;           // Error: CRC mismatch
        }
        else
        {
            *state = READ_CRC_LOW;
        }

        break;
    case READ_CRC_LOW:
        /* code */
        if (byte != (*crc & 0xFF))
        {
            *state = WAIT_START; // Reset state on error
            return PARSER_ERROR_CRC_MISMATCH;           // Error: CRC mismatch
        }
        else
            *state = WAIT_END;
        break;
    case WAIT_END:
        /* code */
        if (byte == PACKER_END_BYTE)
        {
            // Successfully received a full packet
            *state = WAIT_START;
            ret = PARSER_SUCCESS;
            // return 0; // Indicate successful packet reception
        }
        else
        {
            *state = WAIT_START; // Reset state on error
            return PARSER_ERROR_INVALID_END_BYTE;           // Error: Invalid end byte
        }
        break;

    default:
        break;
    }
    return ret;
}
