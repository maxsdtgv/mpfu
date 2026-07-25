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
// Arm the autonomous EEPROM->flash upgrade: sets IsExtUpgrade + ExtAddr in the
// flags row (read-modify-write) and resets the MCU so the bootloader performs
// the upgrade on the next start. This is the ONLY sanctioned way to modify the
// flags row; WriteAppBlock still refuses plain writes to it.
#define SET_EXT_UPGRADE		0x16

// Row that holds the application's relocated reset vector. StartApp() jumps here.
// It is a dedicated row ABOVE the flags row so a bad vector wraps past 0x3FFF to
// 0x0000 (-> GOTO bootloader -> self-recovery) instead of falling into flags.
#define APP_RESET_VECTOR	0x3FE0

// Memory-protection / layout limits used by WriteAppBlock():
//   - the reset row 0x0000-0x0003 holds the bootloader's own reset trampoline
//     (MOVLP;GOTO pair, possibly two-step through 0x0002) and is preserved
//   - the bootloader CODE 0x3800-0x3FBF must never be overwritten by an app
//   - the flags row (0x3FC0) is owned by the bootloader and refused to apps
//   - the app-vector row (0x3FE0) is written by the host like any other block
#define RESET_ROW_ADDR      0x0000
#define RESET_VECTOR_NWORDS 4        // words 0x0000-0x0003 (BL trampoline)
#define BL_CODE_START       0x3800
#define BL_CODE_END         0x3FBF

// Device ID (DEVID) word in configuration space; read with FLASH_Read().
#define DEVID_ADDR          0x8006

// --- Unified firmware-image format v2 (see host_part/cpp/imagev2.h) ----------
// Header (64 bytes) then block_count entries of {addr(2), data(64)} = 66 bytes.
// All multi-byte fields big-endian.
#define IMGV2_HEADER_SIZE     64
#define IMGV2_FORMAT_VERSION  0x02
#define IMGV2_DEVICE_ANY      0xFFFF
#define IMGV2_OFF_MAGIC       0   // 'M','P','F','U'
#define IMGV2_OFF_VERSION     4
#define IMGV2_OFF_DEVICE_ID   5   // 2 bytes
#define IMGV2_OFF_BLOCK_COUNT 7   // 2 bytes
#define IMGV2_OFF_CRC         9   // fletcher16 over the blocks area (2 bytes)
#define IMGV2_BLOCK_ADDR_BYTES 2
#define IMGV2_BLOCK_SIZE       (IMGV2_BLOCK_ADDR_BYTES + MAX_BLOCK_BYTES_SIZE) // 66

// ExtUpgrade status codes written to BLFlags.StatusCodeExtUpgrade:
#define EXTUP_STATUS_OK        0x00
#define EXTUP_STATUS_BAD_MAGIC 0x01
#define EXTUP_STATUS_BAD_CRC   0x02
#define EXTUP_STATUS_BAD_DEVICE 0x03
#define EXTUP_STATUS_BAD_VERSION 0x04

// Optional integrity/identity checks. Each costs flash; the bootloader budget on
// the 16F1789 is tight (code region 0x3800-0x3FBF = 1984 words). Enable as flash
// allows; measure with build.sh. Kept OFF by default.
//#define USE_FLETCHER          // verify image Fletcher-16 before flashing
//#define USE_DEVICE_ID_CHECK   // verify image device_id vs chip DEVID (0x8006)
//#define USE_VERSION_CHECK     // verify image format_version == IMGV2_FORMAT_VERSION

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
bool SetExtUpgrade(uint8_t*, uint8_t*);
void StartApp(void);
void ExtUpgrade(void);

#ifdef	__cplusplus
}
#endif

#endif	/* BOOTLOADER_H */

