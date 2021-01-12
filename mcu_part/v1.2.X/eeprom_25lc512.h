/* 
 * File:   eeprom_25lc512.h
 * Author: mvysochinenko
 *
 * Created on September 16, 2020, 11:30 PM
 */

#ifndef EEPROM_25LC512_H
#define	EEPROM_25LC512_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

#define	SPI_COMMAND_READ	0x03	// Read data from memory array beginning at selected address
#define	SPI_COMMAND_WRITE	0x02	// Write data to memory array beginning at selected address
#define	SPI_COMMAND_WREN	0x06	// Set the write enable latch (enable write operations)
#define	SPI_COMMAND_WRDI	0x04	// Reset the write enable latch (disable write operations)
#define	SPI_COMMAND_PE		0x42 	// Page Erase – erase one page in memory array
#define	SPI_COMMAND_RDSR	0x05 	// Read STATUS register

#define EEPROM_25LC512_BLOCK_SIZE_BYTES 0x80 // Block size in EEPROM 25LC512 
#define MAX_BYTES_TO_READ_PER_BLOCK 	0x40 // Will be read 64 bytes (32 words)
#define MAX_BYTES_TO_WRITE_PER_BLOCK	0x43 // Will be write 64 bytes (32 words) + 3 system flags
#define DELAY_TO_PROCEED_COMMAND		0x0A // Delay on pin !CS to proceed command

#define Enable_serial_eeprom() do {IO_RA0_SetLow();} while(0)
#define Disable_serial_eeprom() do {IO_RA0_SetHigh();} while(0)

bool ReadFromSerialEEPROM(uint8_t *, uint8_t *);
bool WriteToSerialEEPROM(uint8_t *, uint8_t *);


#ifdef	__cplusplus
}
#endif

#endif	/* EEPROM_25LC512_H */
