/*
 * debug_print.c
 *
 *  Created on: Oct 31, 2023
 *      Author: nguye
 */

#include "debug_print.h"
#include "stm32f4xx_hal.h"


extern UART_HandleTypeDef huart2;
//extern HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
//int _fstat (int file, struct stat * st) {
//    errno = -ENOSYS;
//    return -1;
//}

void uart_putc(int c)
{
//#define uart2_txdata    (*(volatile uint32_t*)(0x10013000)) // uart0 txdata register
//#define UART_TXFULL             (1 << 31)  // uart0 txdata flag
//    while ((uart0_txdata & UART_TXFULL) != 0) { }
//    uart0_txdata = c;
	HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

//int _write (int file, char * ptr, int len) {
//    extern int uart_putc(int c);
//    int i;
//
//    /* Turn character to capital letter and output to UART port */
//    for (i = 0; i < len; i++) uart_putc((int)*ptr++);
//    return 0;
//}

void print(const char * str)
{
	    do
	    {
	    	uart_putc((int)*str);
            if((*str)== '\n' || (*str) == 0)
            {
            	break;
            }
            str++;
	    }while(1);
}





