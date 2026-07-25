
#include "internal_flash.h"

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



// Perform the EECON2 unlock sequence and set WR. Two variants:
//
//  LoadLatch()       - used while LWLO=1 (loading write latches). Per the
//                      datasheet (sec 12.3): "The processor does not stall when
//                      LWLO = 1, loading the write latches." So NO delay is
//                      needed here. Previously this waited DELAY_WRITE_FLASH ms
//                      on every one of the 32 latches, making a row write ~3 s.
//
//  CommitFlashWrite()- used for the actual write cycles (row erase, and the
//                      final word with LWLO=0). Here the CPU halts for ~2 ms
//                      while the write/erase happens, so we keep the delay.
static void LoadLatch(void){
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;  // load latch (does not start a write while LWLO=1)
    NOP();
    NOP();
    return;
}

void UnlockFlashWrite(void){   // "commit" variant: real write/erase, needs delay
    EECON2 = 0x55;      // Start of required sequence to initiate write/erase
    EECON2 = 0xAA;
    EECON1bits.WR = 1;  // Set WR bit to begin write
    NOP();
    NOP();
    __delay_ms(DELAY_WRITE_FLASH);
    return;
}

uint16_t FLASH_Read(uint16_t addr){
    uint8_t INR_state = INTCON;
    INTCONbits.GIE = 0; // Disable interrupts

    EECON1bits.EEPGD = 1; // Select Program Memory
    EECON1bits.FREE = 0; // DeSpecify an erase operation   

        EEADRH = (uint8_t)((addr & 0xFF00)>>8);
        EEADRL = (uint8_t)(addr & 0x00FF);

    if ((uint8_t)((addr & 0xFF00) >> 8) == 0x80){
        EECON1bits.CFGS = 1; // Select Configuration Space
    } else {
        EECON1bits.CFGS = 0; // Do not select Configuration Space
    }
    
    EECON1bits.RD = 1; // Initiate read
    NOP();
    NOP();

    EECON1bits.EEPGD = 0; // DeSelect Program Memory
    EECON1bits.CFGS = 0; // Do not select Configuration Space

    INTCON = INR_state; // Restore interrupts

    return ((uint16_t)((EEDATH << 8) | EEDATL));
}

bool FLASH_Write(uint8_t *buffer){
    uint8_t i = 0; 
    uint8_t INR_state = INTCON;
    uint16_t writeAddr = 0;

    INTCONbits.GIE = 0; // Disable interrupts

    // NOTE (flash row behaviour / alignment requirement):
    // On PIC16F1 the program-memory write engine latches a whole row of
    // 32 words. The row is selected by EEADRH:EEADRL[13:5]; only the low
    // 5 bits EEADRL[4:0] pick one of the 32 write latches inside that row.
    // During the latch-loading loop below the address is incremented, but
    // if a block does NOT start on a 32-word boundary (0x20) the low bits
    // wrap around inside the SAME row instead of advancing to the next one
    // (looks like the address "does not roll over" / "jumps to +1").
    // Workaround: the host always sends blocks of 32 words aligned to 0x20
    // (see fw_converter.cpp and docs/BL_protocol.txt "aligned to x32").
    // The XC8 memory model reserves the BL area accordingly:
    //   default,-0-37FF,-3FE0-3FFF
    writeAddr = (uint16_t)(((buffer[2] << 8) & 0xFF00) | (buffer[3] & 0x00FF));
        EEADRH = (uint8_t)((writeAddr & 0xFF00)>>8);
        EEADRL = (uint8_t)(writeAddr & 0x00FF);

    EECON1bits.EEPGD = 1; // Select Program Memory
    EECON1bits.WREN = 1; // Enable writes
    EECON1bits.WRERR = 0; // Clear errors flag

    if (buffer[2] == 0x80){
            EECON1bits.CFGS = 1; // Select Configuration Space
            EEDATH = buffer[4];
            EEDATL = buffer[5];
    } else {
            // Block erase sequence
            EECON1bits.CFGS = 0; // Do not select Configuration Space
            EECON1bits.FREE = 1; // Specify an erase operation

            UnlockFlashWrite();

            EECON1bits.FREE = 0; // DeSpecify an erase operation

    EECON1bits.LWLO = 1; // Only Load Write Latches        

        for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
        EEADRH = (uint8_t)((writeAddr & 0xFF00)>>8); 
        EEADRL = (uint8_t)(writeAddr & 0x00FF);

            EEDATH = buffer[i+4];
            EEDATL = buffer[i+5];

            LoadLatch();      // load latch only (LWLO=1): no write yet, no delay

            writeAddr++;

        }
    EECON1bits.LWLO = 0;        // End Only Load Write Latches
    
    }

    UnlockFlashWrite();     // final commit (LWLO=0): actual write, ~2ms delay

    EECON1bits.WREN = 0;        // Disable writes
    EECON1bits.EEPGD = 0;       // DeSelect Program Memory
    EECON1bits.CFGS = 0;        // Do not select Configuration Space
    INTCON = INR_state; // Restore interrupts

    if (EECON1bits.WRERR){ 
        return false;
    } else {
        return true;
    }
    
}

void ReadBootloaderFlags(void){
    uint8_t i = 0;
    uint16_t buf[MAX_BLOCK_SIZE];
    uint16_t def_addr = FLAGS_VECTOR;

        for (i = 0; i != MAX_BLOCK_SIZE; i ++){
            buf[i] = FLASH_Read(def_addr);   
            def_addr++;
        }

    // buf[] is indexed by WORD offset from FLAGS_VECTOR. Only the low byte of
    // each flag word is significant (FLAG_SET_BYTE == set).
    if ((uint8_t)(buf[FLAG_OFF_IS_BL_START]    & 0x00FF) == FLAG_SET_BYTE){ BLFlags.IsBLStart = true;}
    if ((uint8_t)(buf[FLAG_OFF_IS_EXT_UPGRADE] & 0x00FF) == FLAG_SET_BYTE){ BLFlags.IsExtUpgrade = true;}

    BLFlags.StartAddrExtUpgrade = (uint16_t)(((buf[FLAG_OFF_EXT_ADDR_H]    << 8) & 0xFF00) | (buf[FLAG_OFF_EXT_ADDR_L]    & 0x00FF));
    BLFlags.StatusCodeExtUpgrade = (uint8_t)(buf[FLAG_OFF_EXT_STATUS] & 0x00FF);
    return;
}

bool WriteBootloaderFlags(void){
    uint8_t i = 0;
    uint8_t buf[MAX_BLOCK_BYTES_SIZE + 4];
    uint16_t dbyte = 0;
    uint16_t def_addr = FLAGS_VECTOR;
    bool res = false;

    // Read-modify-write the whole flags row: first read the current 32 words
    // into the FLASH_Write frame buffer (data starts at FRAME_DATA_OFFSET),
    // preserving everything. The flags row (0x3FC0) now holds ONLY flags; the
    // application reset vector lives in its own row (0x3FE0).
        for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
            dbyte = FLASH_Read(def_addr);   
            buf[FRAME_DATA_OFFSET + i]     = (uint8_t)((dbyte & 0xFF00) >> 8);
            buf[FRAME_DATA_OFFSET + i + 1] = (uint8_t)(dbyte & 0x00FF);
            def_addr++;
        }

    // Frame header: address of the row to write (len/cmd are set by convention
    // elsewhere; FLASH_Write only uses buf[2..3] as the target address).
    buf[2] = (uint8_t)((FLAGS_VECTOR & 0xFF00) >> 8);
    buf[3] = (uint8_t)(FLAGS_VECTOR & 0x00FF);

    // Overwrite only the flag low-bytes we own; everything else stays as read.
    buf[WORD_LO_BYTE(FLAG_OFF_IS_BL_START)]    = BLFlags.IsBLStart    ? FLAG_SET_BYTE : FLAG_CLEAR_BYTE;
    buf[WORD_LO_BYTE(FLAG_OFF_IS_EXT_UPGRADE)] = BLFlags.IsExtUpgrade ? FLAG_SET_BYTE : FLAG_CLEAR_BYTE;
    buf[WORD_LO_BYTE(FLAG_OFF_EXT_STATUS)]     = BLFlags.StatusCodeExtUpgrade;

    res = FLASH_Write(buf);
  
    return res;
}
