/**
 * @file uart_manager.c
 * @brief UART Manager for handling TX queue, RX queue, and polling queue.
 *        This module manages UART communication, including sending commands
 *        via the TX queue and processing data received in the RX queue.
 *
 * @author Nguyen Tiem
 * @date 2023
 *
 * @details
 * This file is part of the STM32F103 fingerprint project. It provides
 * functionality to manage UART communication, including:
 * - TX queue for transmitting data
 * - RX queue for receiving data
 * - Polling queue for parsing and processing commands
 * - Integration with a packer for sending commands
 *
 * @note Ensure proper initialization of UART peripherals before using this module.
 */

#include "uart_manager.h"
#include "config.h"
#include "log.h"
#include "queue.h"

static uint8_t uart_tx_buffer[UART_TX_COMMAND_SIZE];
static uint8_t uart_rx_buffer[UART_RX_COMMAND_SIZE];

static uint8_t uart_tx_queue_buffer[UART_TX_COMMAND_SIZE];
static uint8_t uart_rx_queue_buffer[UART_RX_COMMAND_SIZE];

static volatile uint8_t uart_tx_in_progress = 0;
static queue uart_tx_queue;
static queue uart_rx_queue;
// lower level function to push data to tx queue
static void push_to_tx_queue(uint8_t *data, uint16_t length);
static parser_success_cb start_send_uart;
/// @brief
/// @param on_unpack_success
/// @param on_unpack_fail
void uart_manager_init(parser_success_cb on_unpack_success, parser_fail_cb on_unpack_fail, parser_success_cb send_usart)
{
    queue_init(&uart_tx_queue, uart_tx_queue_buffer, sizeof(uart_tx_queue_buffer));
    queue_init(&uart_rx_queue, uart_rx_queue_buffer, sizeof(uart_rx_queue_buffer));
    uart_parser_init(on_unpack_success, on_unpack_fail, push_to_tx_queue);
    start_send_uart = send_usart;
}

// push data from ISR uart rx callback
/// @param data
void onReceiveData(uint8_t *data, uint16_t length)
{
    queue_push(&uart_rx_queue, (uint8_t *)data, length);
}

/// @brief
/// @param data
/// @param length
static void push_to_tx_queue(uint8_t *data, uint16_t length)
{
    uint16_t pushed_length = queue_push(&uart_tx_queue, data, length);
    if (pushed_length < length)
    {
        log_print("UART TX queue overflow. Some data may be lost.\r\n");
    }
}

// call send data from task
void uart_manager_send_data(uint8_t *data, uint16_t length)
{
    static volatile uint8_t sending = 0;
    if (sending == 0)
    {
        sending = 1;
        uart_parser_pack_data((char *)data, length, uart_tx_buffer, sizeof(uart_tx_buffer));
        sending = 0;
    }
    else
    {
        log_print("UART TX queue is busy. Data not sent.\r\n");
    }
}

uint8_t uart_rx[255];
void uart_control()
{
//	uint8_t recv_byte;
    // handle rx queue
    if(queue_get_data_length(&uart_rx_queue) > 0)
    {
//        log_printf("Processing data from UART RX queue...%d\r\n", queue_get_data_length(&uart_rx_queue));
    	uint16_t len = queue_pop(&uart_rx_queue, uart_rx, 255);
//    	 log_printf("poll %d \n", len);
        uart_parser_unpack_data((const char*)uart_rx, len, uart_rx_buffer, sizeof(uart_rx_buffer));
//    	while(queue_pop_byte(&uart_rx_queue, &recv_byte) == QUEUE_SUCCESS){
//    		uart_parser_unpack_data_byte(recv_byte, uart_rx_buffer, sizeof(uart_rx_buffer));
//    	}

    }

    //
    if (queue_get_data_length(&uart_tx_queue) > 0)
    {
        if (uart_tx_in_progress == 0)
        {
            uart_tx_in_progress = 1;
            uint16_t to_send = queue_pop(&uart_tx_queue, uart_tx_buffer, sizeof(uart_tx_buffer));
            if (start_send_uart)
            {
                start_send_uart(uart_tx_buffer, to_send);
            }
        }
        log_print("Sending data from UART TX queue...\r\n");
    }
}

void onSendDone()
{
    uart_tx_in_progress = 0;
}
// check sending -> send done

// receiving -> receive done -> push to rx queue -> uart control -> parse data from rx queue
