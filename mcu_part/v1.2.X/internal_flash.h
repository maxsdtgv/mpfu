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

// --- Bootloader flags row layout ---------------------------------------------
// The flags live in the 32-word row starting at FLAGS_VECTOR (0x3FE0). Each
// field below is a WORD OFFSET from FLAGS_VECTOR. Only the low byte of each
// flag word is used (0x00 = set/true, anything else = clear/false).
#define FLAG_OFF_IS_BL_START     0   // 0x3FE0
#define FLAG_OFF_IS_EXT_UPGRADE  1   // 0x3FE1
#define FLAG_OFF_EXT_ADDR_H      2   // 0x3FE2
#define FLAG_OFF_EXT_ADDR_L      3   // 0x3FE3
#define FLAG_OFF_EXT_NBLOCKS_H   4   // 0x3FE4
#define FLAG_OFF_EXT_NBLOCKS_L   5   // 0x3FE5
#define FLAG_OFF_EXT_STATUS      6   // 0x3FE6

#define FLAG_SET_BYTE            0x00   // low byte value meaning "flag is set"
#define FLAG_CLEAR_BYTE         0xFF   // low byte value meaning "flag is clear"

// FLASH_Write() expects a buffer laid out as a WRITE frame:
//   buf[0]=len, buf[1]=cmd, buf[2]=addrH, buf[3]=addrL, then data bytes at buf[4]+.
// So a given word offset in the row maps to these byte indices in that buffer:
#define FRAME_DATA_OFFSET        4
#define WORD_HI_BYTE(word_off)  (FRAME_DATA_OFFSET + (word_off)*2)
#define WORD_LO_BYTE(word_off)  (FRAME_DATA_OFFSET + (word_off)*2 + 1)

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

