/* 
 * File:   bootloader.h
 * Author: mvysochinenko
 *
 * Created on September 16, 2020, 11:29 PM
 */

#ifndef BOOTLOADER_H
#define	BOOTLOADER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../../main.h"

// Parameters definitions

#define BL_MAX_RECV_DATA	68
#define BL_MAX_SEND_DATA	66

//=============    SUPPORTED COMMANDS ================
//#define MAJOR_VERSION		0x01
//#define MINOR_VERSION		0x01
//=============    SUPPORTED COMMANDS ================
#define ERROR_CODE			0xFF
//#define SUCCESS_CODE		0xEE
//#define PING_REQUEST		0x01

#define READ_FROM_MEM		0x02
//#define ERASE_ROW_MEM		0x03
#define WRITE_TO_MEM		0x04
#define START_APPLICATION	0x0F

#define READ_FROM_SERIAL_EEPROM		0x12
#define WRITE_TO_SERIAL_EEPROM		0x14
#define RESET_VECTOR_APP	0x3FFC

#define START_APPLICATION	0x0F
//====================================================

#define _str(x)			#x
#define str(x)			_str(x)

// Function registration

bool KeyBLRequired(void);
void ClearArray(uint8_t*, uint8_t);
bool DefineError(uint8_t*);
//bool PingRequest(uint8_t*, uint8_t*);
//bool EraseRowMem(uint8_t*, uint8_t*);
bool ReadFromMem(uint8_t*, uint8_t*);
bool WriteToMem(uint8_t*, uint8_t*);
void StartApp(void);
void ExtUpgrade(void);

#ifdef	__cplusplus
}
#endif

#endif	/* BOOTLOADER_H */

