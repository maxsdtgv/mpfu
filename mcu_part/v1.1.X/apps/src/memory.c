
/* 
 * File:   memory.c
 * Author: mvysochinenko
 *
 * Created on January 7, 2020, 6:51 PM
 */

#include "../api/memory.h"

//bool FLASH_Erase(uint8_t *buffer){    // According to flow described in p.122 Microchip datasheet
//    uint8_t INR_state;
//    INR_state = INTCONbits.GIE;
//
//    INTCONbits.GIE = 0; // Disable interrupts
//
//    //EEADRH = (uint8_t)((addr & 0xFF00) >> 8);
//    //EEADRL = (uint8_t)(addr & 0x00FF);
//
//    EEADRH = buffer[2];
//    EEADRL = buffer[3];
//
//    EECON1 = 0x94;
//    //EECON1bits.EEPGD = 1; // Select Program Memory
//    //EECON1bits.CFGS = 0; // Do not select Configuration Space
//    //EECON1bits.FREE = 1; // Specify an erase operation
//    //EECON1bits.WREN = 1; // Enable writes
// 
//    EECON2 = 0x55;      // Start of required sequence to initiate erase
//    EECON2 = 0xAA;
//    EECON1bits.WR = 1;  // Set WR bit to begin erase
//    NOP();
//    NOP();
//    
//    EECON1bits.WREN = 0;    // Disable writes
//    INTCONbits.GIE = INR_state; // Restore interrupts
//    
//
//    return true;
//}



void UnlockFlashWrite(){
    EECON2 = 0x55;      // Start of required sequence to initiate erase
    EECON2 = 0xAA;
    EECON1bits.WR = 1;  // Set WR bit to begin erase
    NOP();
    NOP();
}

uint16_t FLASH_Read(uint16_t addr){
    uint8_t INR_state = INTCONbits.GIE;
    INTCONbits.GIE = 0; // Disable interrupts

        EEADRH = (uint8_t)((addr & 0xFF00)>>8);
        EEADRL = (uint8_t)(addr & 0x00FF);

    if ((uint8_t)((addr & 0xFF00) >> 8) == 0x80){
        EECON1bits.CFGS = 1; // Select Configuration Space
    } else {
        EECON1bits.CFGS = 0; // Do not select Configuration Space
    }

    EECON1bits.EEPGD = 1; // Select Program Memory
    
    EECON1bits.RD = 1; // Initiate read
    NOP();
    NOP();
    
    INTCONbits.GIE = INR_state; // Restore interrupts
    return ((uint16_t)((EEDATH << 8) | EEDATL));
}

bool FLASH_Write(uint8_t *buffer){
    uint8_t i, INR_state = INTCONbits.GIE;
    uint16_t writeAddr;

    INTCONbits.GIE = 0; // Disable interrupts

    writeAddr = (uint16_t)(((buffer[2] << 8) & 0xFF00) | (buffer[3] & 0x00FF));
        EEADRH = (uint8_t)((writeAddr & 0xFF00)>>8);
        EEADRL = (uint8_t)(writeAddr & 0x00FF);

    EECON1bits.EEPGD = 1; // Select Program Memory
    EECON1bits.WRERR = 0; // EEPROM Error Flag bit
    EECON1bits.WREN = 1; // Enable writes

    if (buffer[2] == 0x80){
            EECON1bits.CFGS = 1; // Select Configuration Space
            EEDATH = buffer[4];
            EEDATL = buffer[5];
    } else {
        
    // Block erase sequence
    EECON1bits.CFGS = 0; // Do not select Configuration Space
    EECON1bits.FREE = 1; // Specify an erase operation
 
    UnlockFlashWrite();
    __delay_ms(DELAY_WRITE_FLASH);

    if (EECON1bits.FREE){ 
            return false;
    }

    EECON1bits.LWLO = 1; // Only Load Write Latches        
        for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
        EEADRH = (uint8_t)((writeAddr & 0xFF00)>>8); 
        EEADRL = (uint8_t)(writeAddr & 0x00FF);

            EEDATH = buffer[i+4];
            EEDATL = buffer[i+5];

            UnlockFlashWrite();

            writeAddr++;

        }
    EECON1bits.LWLO = 0; // End Only Load Write Latches
    
    }

    UnlockFlashWrite();
    __delay_ms(DELAY_WRITE_FLASH);
    
    //EECON1bits.CFGS = 0;          // Do not select Configuration Space
    EECON1bits.WREN = 0;            // Disable writes
    INTCONbits.GIE = INR_state;     // Restore interrupts
    if (EECON1bits.WRERR){ 
        return false;
    } else {
        return true;
    }
    
}

void ReadBootloaderFlags(void){
    uint8_t i = 0;
    uint16_t buf[MAX_BLOCK_SIZE];
    uint16_t dbyte = 0;
    uint16_t def_addr = FLAGS_VECTOR;

        for (i = 0; i != MAX_BLOCK_SIZE; i ++){
            buf[i] = FLASH_Read(def_addr);   
            def_addr++;
        }

    if ((uint8_t)(buf[0] & 0x00FF) == 0x00){ BLFlags.IsBLStart = true;}
    if ((uint8_t)(buf[1] & 0x00FF) == 0x00){ BLFlags.IsExtUpgrade = true;}
    BLFlags.StartAddrExtUpgrade = (uint16_t)(((buf[2] << 8) & 0xFF00) | (buf[3] & 0x00FF));
    BLFlags.NumBlocksExtUpgrade = (uint16_t)(((buf[4] << 8) & 0xFF00) | (buf[5] & 0x00FF));
    BLFlags.StatusCodeExtUpgrade = (uint8_t)(buf[6] & 0x00FF);

}

void WriteBootloaderFlags(void){
    uint8_t i = 0;
    uint8_t buf[MAX_BLOCK_BYTES_SIZE + 4];
    uint16_t dbyte = 0;
    uint16_t def_addr = FLAGS_VECTOR;

        for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
            dbyte = FLASH_Read(def_addr);   
            buf[i+4] = (uint8_t)((dbyte & 0xFF00) >> 8);
            buf[i+5] = (uint8_t)(dbyte & 0x00FF);
            def_addr++;
        }

        buf[2] = (uint8_t)((FLAGS_VECTOR & 0xFF00) >> 8);
        buf[3] = (uint8_t)(FLAGS_VECTOR & 0x00FF);

    if (BLFlags.IsBLStart == true){
        buf[5] = 0x00;
    } else {
        buf[5] = 0xFF;
    }

    if (BLFlags.IsExtUpgrade == true){
        buf[7] = 0x00;
    } else {
        buf[7] = 0xFF;
    }

    buf[17] = BLFlags.StatusCodeExtUpgrade;


    FLASH_Write(buf);


}
