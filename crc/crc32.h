#ifndef CRC32_H
#define CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
void crc32_init(uint32_t *crc);
uint32_t crc32_cal(uint8_t *input, uint32_t len);
void crc32_frag_cal(uint32_t *crc, uint8_t *buff, uint32_t len);
void crc32_byte_cal(uint32_t *crc, uint8_t byte);
#ifdef __cplusplus
}
#endif

#endif