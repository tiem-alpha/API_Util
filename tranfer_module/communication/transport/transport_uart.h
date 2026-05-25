#ifndef TRANSPORT_UART_H
#define TRANSPORT_UART_H

#include <stdint.h>
#include "../communication_manager.h"

/**
 * Bind transport_uart to a comm_transport_t struct.
 * Call before comm_init().
 */
void transport_uart_init(comm_transport_t *transport);

/* --- Hooks to be called from your UART HAL / ISR --- */

/**
 * Call from your UART RX-complete ISR or DMA callback.
 * Forwards received bytes into the comm manager's RX queue.
 */
void transport_uart_on_receive(uint8_t *data, uint16_t len);  /* call from RX ISR/DMA */

/**
 * Call from your UART TX-complete ISR or DMA callback.
 * Clears tx_busy and fires COMM_EVENT_TX_DONE.
 */
void transport_uart_on_send_done(void);

#endif /* TRANSPORT_UART_H */
