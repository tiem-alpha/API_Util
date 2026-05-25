#ifndef COMM_CRC_H
#define COMM_CRC_H

#include <stdint.h>

/* CRC-16/MCRF4XX: poly=0x1021, init=0xFFFF, RefIn=true, RefOut=true, XorOut=0x0000 */

uint16_t crc16_mcrf4xx(const uint8_t *data, uint16_t len);
uint16_t crc16_mcrf4xx_update(uint16_t crc, uint8_t byte);

#endif /* COMM_CRC_H */
