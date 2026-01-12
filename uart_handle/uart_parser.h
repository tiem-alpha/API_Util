#ifndef UART_PARSER_H
#define UART_PARSER_H
#ifdef __cplusplus
extern "C"
{
#endif
#include"data_type.h"

    typedef void (*parser_success_cb)(uint8_t *, uint16_t);
    typedef void (*parser_fail_cb)(uint8_t);

    void uart_parser_init(parser_success_cb on_success, parser_fail_cb on_error, parser_success_cb on_pack_success_cb);
    void uart_parser_pack_data(const char *data, uint16_t length, uint8_t *tx_buffer, uint16_t tx_buffer_size);
    void uart_parser_unpack_data(const char *data_input, uint16_t length, uint8_t *rx_buffer, uint16_t rx_buffer_size);
    uint8_t uart_parser_unpack_data_byte(const char data, uint8_t *rx_buffer, uint16_t rx_buffer_size);

#ifdef __cplusplus
}
#endif
#endif // UART_PARSER_H