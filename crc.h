/*
 * crc.h
 *
 *  Created on: Nov 28, 2024
 *      Author: TiemNV1
 */

#ifndef INC_CRC_H_
#define INC_CRC_H_

void crc16_init(uint16_t *crc);
void crc16_cal(uint8_t *buff, uint16_t len);
void crc16_frag_cal(uint8_t *crc, uint8_t *buff, uint8_t len);


#endif /* INC_CRC_H_ */
