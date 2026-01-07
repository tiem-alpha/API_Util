/*
 * packer.h
 *
 *  Created on: Nov 28, 2024
 *      Author: TiemNV1
 */

#ifndef INC_PACKER_H_
#define INC_PACKER_H_
#include"tlv.h"

#define MAX_LENGTH_DATA 128

enum CMD{
        NONE,
SYSTEM_INFOR, // factory data
STATUS,/// open/ close / on/off, PARITAL
SCHEDULE,
DURATION,
IDICATOR,
BUZZER,
ACCESS_CODE,
SOFT_RESET,
POWER,
FACTORY_RESET,
OTA,
};

enum  PID{
APP,
TEST,
DEBG,
};

enum CHANNEL{
UNKNOW,
BLE,
WIFI,
ZIGBEE,
MATTER,
ZWAVE,
LORA,
KEY_PAD,
WALL_STATION,
MANUAL,
};

enum MSG_TYPE{
COMMAND,
REQUEST,
RESPONSE,
EVENT,
};


typedef struct{
	uint8_t type_msg; //cmd/ event/request/response
    uint8_t type_cmd;
    uint8_t len;
    uint8_t value[MAX_LENGTH_DATA];
}tlv;

typedef struct{
	uint8_t pid; // debug/ test / app
	uint8_t channel;
	uint8_t command_code;
	uint8_t src; // ID user
	tlv command;
}message;

enum validate_state{
WAIT_START,
WAIT_PID,
WAIT_CHANNEL,
WAIT_COMMAND_CODE, // command
WAIT_SRC,
WAIT_MSG_TYPE,
WAIT_CMD_TYPE,
WAIT_LENGTH,
WAIT_VALUE,
WAITE_CRC,
};
#endif /* INC_PACKER_H_ */
