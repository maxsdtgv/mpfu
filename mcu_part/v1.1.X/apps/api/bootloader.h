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

#define BL_MAX_RECV_DATA	66
#define BL_MAX_SEND_DATA	66

//=============    SUPPORTED COMMANDS ================
#define MAJOR_VERSION		0x01
#define MINOR_VERSION		0x01
//=============    SUPPORTED COMMANDS ================
#define ERROR_CODE			0xFF
#define SUCCESS_CODE		0xEE
//#define PING_REQUEST		0x01
#define READ_FROM_MEM		0x02	

#define START_APPLICATION	0x0F
//====================================================

// Function registration

void ClearArray(uint8_t*);

bool DefineError(uint8_t*);

//bool PingRequest(uint8_t*, uint8_t*);

bool ReadFromMem(uint8_t*, uint8_t*);

#ifdef	__cplusplus
}
#endif

#endif	/* BOOTLOADER_H */

