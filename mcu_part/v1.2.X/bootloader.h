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

// --- EEPROM firmware-image format (see host_part/cpp/eeimage.h) --------------
// Header (64 bytes) then dense data. All multi-byte fields big-endian.
#define EEIMG_HEADER_SIZE     64
#define EEIMG_OFF_MAGIC       0   // 'M','P','F','U'
#define EEIMG_OFF_VERSION     4
#define EEIMG_OFF_FLASH_ADDR  5   // destination flash WORD address (2 bytes)
#define EEIMG_OFF_DATA_LEN    7   // number of data bytes (2 bytes, multiple of 64)
#define EEIMG_OFF_CRC         9   // fletcher16 over data (2 bytes)

// ExtUpgrade status codes written to BLFlags.StatusCodeExtUpgrade (0x3FE6):
#define EXTUP_STATUS_OK        0x00
#define EXTUP_STATUS_BAD_MAGIC 0x01
#define EXTUP_STATUS_BAD_CRC   0x02

// Optional Fletcher-16 integrity check of the EEPROM image before flashing.
// DISABLED by default on the PIC16F1789 to save flash: enabling it adds ~130
// words and requires extending the bootloader ROM region below 0x3800 (e.g.
// -mrom=default,-0-35FF,-3FE0-3FFF). On this part, a Wi-Fi-delivered image can
// instead be verified by reading the EEPROM back over the same link on the host.
// Enable on parts with more flash (and extend the BL region accordingly).
//#define USE_FLETCHER

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

