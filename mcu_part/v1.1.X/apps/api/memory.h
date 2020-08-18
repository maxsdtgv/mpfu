/* 
 * File:   memory.h
 * Author: mvysochinenko
 *
 * Created on January 7, 2020, 6:50 PM
 */

#ifndef MEMORY_H
#define	MEMORY_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../../main.h"

#define START_MEM_ADDR 	0x0
#define END_MEM_ADDR 	0x3FFF	
#define MAX_BLOCK_SIZE 	32
#define MAX_MEM_COUNT 	0x4000



uint16_t FLASH_Read(uint16_t);
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

#ifdef	__cplusplus
}
#endif

#endif	/* MEMORY_H */