#include "crc.h"

/* reflect(0x1021, 16) = 0x8408 */
#define CRC_POLY_REFLECTED 0x8408u

uint16_t crc16_mcrf4xx_update(uint16_t crc, uint8_t byte)
{
    uint8_t i;
    for (i = 0u; i < 8u; i++) {
        if ((crc ^ (uint16_t)byte) & 0x01u) {
            crc = (uint16_t)((crc >> 1) ^ CRC_POLY_REFLECTED);
        } else {
            crc >>= 1;
        }
        byte >>= 1;
    }
    return crc;
}

uint16_t crc16_mcrf4xx(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--) {
        crc = crc16_mcrf4xx_update(crc, *data++);
    }
    return crc;
}
