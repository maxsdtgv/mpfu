#include "eeprom_25lc512.h"


bool ReadFromSerialEEPROM(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t i = 0;
    uint8_t buffer[MAX_BYTES_TO_READ_PER_BLOCK];
    spi_modes_t SPI1 = 0;

    SPI_Open(SPI1);

    buffer[0] = SPI_COMMAND_READ;	// Send command to ext eeprom device
    buffer[1] = recv_frame[2];		// Send address, H part
    buffer[2] = recv_frame[3];		// Send address, L part

    Enable_serial_eeprom();

    SPI_WriteBlock(buffer, 0x03);    	// Send block

    ClearArray(buffer, MAX_BYTES_TO_READ_PER_BLOCK);

    SPI_ReadBlock(buffer, MAX_BYTES_TO_READ_PER_BLOCK);
    
    SPI_Close();

	Disable_serial_eeprom();

	send_frame[0] = MAX_BYTES_TO_READ_PER_BLOCK + 2;
	send_frame[1] = READ_FROM_SERIAL_EEPROM;
	for (i = 0; i != MAX_BYTES_TO_READ_PER_BLOCK; i++){
		send_frame[i+2] = buffer[i];
		}

	return true;
}

bool WriteToSerialEEPROM(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t i = 0;
	uint8_t buffer[MAX_BYTES_TO_WRITE_PER_BLOCK];
    spi_modes_t SPI1 = 0;

    SPI_Open(SPI1);

    // NOTE: no explicit Page Erase is needed. Per the 25LC512 datasheet, "A
    // write sequence includes an automatic, self-timed erase cycle. It is not
    // required to erase any portion of the memory prior to a WRITE." The old
    // first-implementation SPI_COMMAND_PE step was removed: it also caused a
    // bug where writing two 64-byte blocks into the same 128-byte page erased
    // the first block. A WREN must immediately precede the WRITE.
    buffer[0] = SPI_COMMAND_WREN;		// Set write-enable latch
    Enable_serial_eeprom();
    SPI_WriteBlock(buffer, 0x01);		// Send block
      Disable_serial_eeprom();
    __delay_ms(DELAY_TO_PROCEED_COMMAND);

    buffer[0] = SPI_COMMAND_WRITE;		// Send command to ext eeprom device
    buffer[1] = recv_frame[2];			// Send address, H part
    buffer[2] = recv_frame[3];			// Send address, L part
	for (i = 3; i != MAX_BYTES_TO_WRITE_PER_BLOCK; i++){
		buffer[i] = recv_frame[i+1];
		}
    Enable_serial_eeprom();
    SPI_WriteBlock(buffer, MAX_BYTES_TO_WRITE_PER_BLOCK);	// Send block
	Disable_serial_eeprom();
    __delay_ms(DELAY_TO_PROCEED_COMMAND);

    //buffer[0] = SPI_COMMAND_WRDI;		// Send command to ext eeprom device
    //Enable_serial_eeprom();
    //SPI_WriteBlock(buffer, 0x01);		// Send block
    //  Disable_serial_eeprom();
    //__delay_ms(DELAY_TO_PROCEED_COMMAND);

	SPI_Close();
	send_frame[0] = 0x02;
	send_frame[1] = SUCCESS_CODE;   // uniform ACK, like WriteToMem (flash)
	return true;
}


