/* 
 * File:   bootloader.h
 * Author: mvysochinenko
 *
 * Created on July 29, 2020, 8:25 PM
 */

#ifndef BOOTLOADER_H
#define	BOOTLOADER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../../main.h"

// Parameters definitions

#define BL_MAX_RECV_DATA		66
#define BL_MAX_SEND_DATA		66

//=============    SUPPORTED COMMANDS ================
#define ERROR_CODE			0xFF
#define SUCCESS_CODE		0xEE
#define PING_REQUEST		0x01
#define PING_REPLY			0x01
#define VERSION_REQUEST		0x02
#define VERSION_REPLY		0x02
#define FLASH_ADDR_SET		0x03
#define FLASH_ADDR_REQ		0x04
#define DATA_ARRAY_SET		0x05
#define DATA_ARRAY_REQ		0x06
#define FLASH_READ_REQ		0x07
#define FLASH_WRITE_REQ		0x08
#define START_APPLICATION	0x0F
//====================================================


// Function registration

void clearArray(uint8_t*);

bool defineError(uint8_t*);

bool pingRequest(uint8_t*, uint8_t*);

#ifdef	__cplusplus
}
#endif

#endif	/* BOOTLOADER_H */

