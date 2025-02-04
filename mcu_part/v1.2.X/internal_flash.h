/* 
 * File:   internal_flash.h
 * Author: mvysochinenko
 *
 */

#ifndef INTERNAL_FLASH_H
#define	INTERNAL_FLASH_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

//#define START_MEM_ADDR    0x0
//#define END_MEM_ADDR      0x3FFF	
#define MAX_BLOCK_SIZE          0x0020
#define MAX_BLOCK_BYTES_SIZE    0x0040    
//#define MAX_MEM_COUNT 	0x4000
#define DELAY_WRITE_FLASH   0x000A   // means 10 ms
#define FLAGS_VECTOR        0x3FE0  

struct {
    bool IsBLStart;
    bool IsExtUpgrade;
    uint16_t StartAddrExtUpgrade;
    uint16_t NumBlocksExtUpgrade;
    uint8_t StatusCodeExtUpgrade;
} BLFlags = {false, false, 0, 0, 0};


//bool FLASH_Erase(uint8_t*);

uint16_t FLASH_Read(uint16_t);

bool FLASH_Write(uint8_t*);
/*
This functio returns word of data located in Flash by defined address.
Example:
    uint16_t addrs = 0x0A00;
    uint8_t i = 0;
    uint16_t datas = 0;

        for (i = 0; i < 32; i++){
            datas = FLASH_Read(addrs);
            addrs++;
            data[0] = datas;
            data[1] = datas>>8;
            UART_data_write(data, sizeof(datas));
        }
*/

void ReadBootloaderFlags(void);

bool WriteBootloaderFlags(void);

#ifdef	__cplusplus
}
#endif

#endif	/* INTERNAL_FLASH_H */

