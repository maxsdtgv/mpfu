/* 
 * File:   bootloader.h
 * Author: mvysochinenko
 *
 */

#ifndef BOOTLOADER_H
#define	BOOTLOADER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

// Parameters definitions

#define BL_MAX_RECV_DATA	68
#define BL_MAX_SEND_DATA	66

//=============    SUPPORTED COMMANDS ================
//#define MAJOR_VERSION		0x01
//#define MINOR_VERSION		0x01
//=============    SUPPORTED COMMANDS ================
#define ERROR_CODE			0xFF
#define SUCCESS_CODE		0xEE
//#define PING_REQUEST		0x01

#define READ_FROM_MEM		0x02
//#define ERASE_ROW_MEM		0x03
#define WRITE_TO_MEM		0x04

#define READ_FROM_SERIAL_EEPROM		0x12
#define WRITE_TO_SERIAL_EEPROM		0x14
#define APP_RESET_VECTOR	0x3FFC // Should be 0x3FFC

// Memory-protection / layout limits used by WriteAppBlock():
//   - the reset row 0x0000-0x0003 holds "GOTO bootloader" and is preserved
//   - the bootloader CODE 0x3800-0x3FDF must never be overwritten by an app
//   - the flags row 0x3FE0-0x3FFF is writable (holds flags + app reset vector)
#define RESET_ROW_ADDR      0x0000
#define RESET_VECTOR_NWORDS 4        // words 0x0000-0x0003
#define BL_CODE_START       0x3800
#define BL_CODE_END         0x3FDF

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
bool WriteAppBlock(uint16_t flash_addr, uint8_t *data64);
void StartApp(void);
void ExtUpgrade(void);

#ifdef	__cplusplus
}
#endif

#endif	/* BOOTLOADER_H */

