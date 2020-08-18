
/* 
 * File:   memory.c
 * Author: mvysochinenko
 *
 * Created on January 7, 2020, 6:51 PM
 */

#include "../api/memory.h"

uint16_t FLASH_Read(uint16_t addr){
    uint8_t INR_state;
    INR_state = INTCONbits.GIE;

    if ((uint8_t)((addr & 0xFF00) >> 8) == 0x80){
        EEADRH = 0x00;
        EECON1bits.CFGS = 1; // Select Configuration Space
    } else {
    	EEADRH = (uint8_t)((addr & 0xFF00) >> 8);
        EECON1bits.CFGS = 0; // Do not select Configuration Space
    }

    EEADRL = (uint8_t)(addr & 0x00FF);

    EECON1bits.EEPGD = 1; // Select Program Memory
    INTCONbits.GIE = 0; // Disable interrupts
    
    EECON1bits.RD = 1; // Initiate read
    NOP();
    NOP();
    
    INTCONbits.GIE = INR_state; // Restore interrupts
    

    return ((uint16_t)((EEDATH << 8) | EEDATL));
}