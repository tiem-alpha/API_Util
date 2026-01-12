#ifndef UART_MANAGER_H
#define UART_MANAGER_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "uart_parser.h"

    void uart_manager_init(parser_success_cb on_unpack_success, parser_fail_cb on_unpack_fail, parser_success_cb send_usart);
    void uart_control();
    void uart_manager_send_data(uint8_t *data, uint16_t length);
    void onReceiveData(uint8_t *data, uint16_t length);
    void onSendDone();
#ifdef __cplusplus
}
#endif
#endif // UART_MANAGER_H