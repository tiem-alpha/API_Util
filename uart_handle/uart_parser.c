#include "uart_parser.h"
#include <string.h>
#include <stdint.h>
#include "crc16.h"
#include "SEGGER_RTT.h"
#include "queue.h"
#include "packer.h"

uint16_t rx_crc;
uint16_t rx_length;
uint16_t rx_offset;
uint8_t rx_state;

parser_success_cb on_unpack_success = NULL;
parser_fail_cb on_unpack_fail = NULL;
parser_success_cb on_pack_success = NULL;
// uint8_t tx_buffer[PACKER_MAX_MESSAGE_LENTH];

void uart_parser_init(parser_success_cb on_success, parser_fail_cb on_error, parser_success_cb on_pack_success_cb)
{
    on_unpack_success = on_success;
    on_unpack_fail = on_error;
    on_pack_success = on_pack_success_cb;
    rx_state = WAIT_START;
    rx_length = 0;
    rx_offset = 0;
    rx_crc = 0;
}

void uart_parser_pack_data(const char *data, uint16_t length, uint8_t *tx_buffer, uint16_t tx_buffer_size)
{
    uint16_t packed_length = 0;
    uint8_t res = pack_data(data, length, tx_buffer, tx_buffer_size, &packed_length);
    if (res != 0)
    {
        SEGGER_RTT_WriteString(0, "Error: Failed to pack data for sending.\n");
        return; // Error in packing data
    }
    SEGGER_RTT_WriteString(0, "Data packed successfully for sending.\n");
    if (on_pack_success)
    {
        on_pack_success(tx_buffer, packed_length);
    }
    return;
}

void uart_parser_unpack_data(const char *data_input, uint16_t length, uint8_t *rx_buffer, uint16_t rx_buffer_size)
{
    for (uint16_t i = 0; i < length; i++)
    {
        uint8_t result = unpack_data_state(data_input[i], rx_buffer, &rx_offset, &rx_length, rx_buffer_size, &rx_crc, &rx_state);
        if (result == PARSER_RUNNING)
        {
        }
        else if (result > PARSER_RUNNING)
        {
            SEGGER_RTT_WriteString(0, "Error: Failed to unpack data from UART.\n");
            if (on_unpack_fail != NULL)
            {
                on_unpack_fail(result);
            }
            // callback error
        }
        else
        {

            SEGGER_RTT_WriteString(0, "Successfully unpacked a complete message from UART.\n");
            if (on_unpack_success != NULL)
            {
                on_unpack_success(rx_buffer, rx_length);
            }
            // callback success full
        }
    }
    return; // No complete message yet
}

uint8_t uart_parser_unpack_data_byte(const char data, uint8_t *rx_buffer, uint16_t rx_buffer_size)
{
    uint8_t result = unpack_data_state(data, rx_buffer, &rx_offset, &rx_length, rx_buffer_size, &rx_crc, &rx_state);
    if (result == PARSER_RUNNING)
    {
    }
    else if (result > PARSER_RUNNING)
    {
        SEGGER_RTT_WriteString(0, "Error: Failed to unpack data from UART.\n");

        if (on_unpack_fail != NULL)
        {
            on_unpack_fail(result);
        }
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Successfully unpacked a complete message from UART.\n");
        // callback success full
        if (on_unpack_success != NULL)
        {
            on_unpack_success(rx_buffer, rx_length);
        }
    }
    return result;
}
