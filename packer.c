/*
 * packer.c
 *
 *  Created on: Nov 28, 2024
 *      Author: TiemNV1
 */

#include"packer.h"
#include "crc.h"

#define START_BYTE 0xAC

static volatile uint8_t state = WAIT_START;
static volatile uint8_t data_idx =0;
static message msg;
static volatile uint16_t crc = 0;
static uint16_t crc_cal = 0;
bool validate_pack(uint8_t comming_byte){
switch(state){
case WAIT_START:
	if(comming_byte == START_BYTE){
		memset(msg, 0, sizeof(msg));
		crc16_init(&crc_cal);
		state = WAIT_PID;
	}
	break;

case WAIT_PID:
if(comming_byte >= APP && comming_byte <= DEBG){
	msg.pid = comming_byte;
	crc16_byte_cal(&&crc_cal, comming_byte);
	state = WAIT_CHANNEL;
} else{
	state = WAIT_START;
}
	break;

case WAIT_CHANNEL:
	if(comming_byte >= UNKNOW && comming_byte <= MANUAL){
		msg.channel = comming_byte;
				crc16_byte_cal(&&crc_cal, comming_byte);
		state = WAIT_COMMAND_CODE;
	} else{
		state = WAIT_START;
	}
	break;

case WAIT_COMMAND_CODE:

		msg.command_code = comming_byte;
		crc16_byte_cal(&&crc_cal, comming_byte);
		state = WAIT_SRC;

	break;

case WAIT_SRC:
	msg.src = comming_byte;
	crc16_byte_cal(&&crc_cal, comming_byte);
	state = WAIT_MSG_TYPE;

	break;

case WAIT_MSG_TYPE:
	if(comming_byte >= COMMAND && comming_byte <= EVENT){
			msg.command.type_msg = comming_byte;
			crc16_byte_cal(&&crc_cal, comming_byte);
			state = WAIT_CMD_TYPE;
		} else{
			state = WAIT_START;
		}
		break;
	break;

case WAIT_CMD_TYPE:
	if(comming_byte >= NONE && comming_byte <= OTA){
				msg.command.type_cmd = comming_byte;
				crc16_byte_cal(&&crc_cal, comming_byte);
				state = WAIT_LENGTH;
			} else{
				state = WAIT_START;
			}
	break;

case WAIT_LENGTH:
	if(comming_byte >= 0 && comming_byte <= MAX_LENGTH_DATA){
					msg.command.len = comming_byte;
					crc16_byte_cal(&&crc_cal, comming_byte);
					data_idx = 0;
					state = WAIT_VALUE;
				} else{
					state = WAIT_START;
				}
	break;

case WAIT_VALUE:
	if(data_idx < msg.command.len){
		msg.command.value[data_idx++] = comming_byte;
		crc16_byte_cal(&&crc_cal, comming_byte);
	}else{
		state = WAIT_START;
		data_idx =0;
	}
	if(data_idx == msg.command.len){
		state = WAITE_CRC;
		data_idx =0;
		crc = 0;
	}
	break;

case WAITE_CRC:
    if(data_idx < 2){
        crc |=(comming_byte &0xFF);
    	data_idx++;
    }else{
    	state = WAIT_START;
    }

    if(data_idx == 2){
    	//check CRC
        if(crc == crc_cal){
          // post event
        }

    	state = WAIT_START;
    }
    crc<<=8;
	break;

default:
	state = WAIT_START;
	break;
}
}
