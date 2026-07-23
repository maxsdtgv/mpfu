#include "bootloader.h"

bool KeyBLRequired(void){
	if (IO_RB0_GetValue() == 1) { 
		return true;
	} 
		return false;
}

void ClearArray(uint8_t *array, uint8_t len){
	uint8_t i = 0;
    for (i = 0; i != len; i++){array[i]=0x00;} // Clear array to send
    return;
}

bool DefineError(uint8_t *send_frame){
	send_frame[0] = 0x02;     		// Number of bytes to send in response
	send_frame[1] = ERROR_CODE;     // Error code
	return true;
}

//bool PingRequest(uint8_t *recv_frame, uint8_t *send_frame){
//	uint16_t dbyte = 0;
//
//	send_frame[0] = 0x04;	// Length of array
//	send_frame[1] = PING_REQUEST;
//	send_frame[2] = MAJOR_VERSION;
//	send_frame[3] = MINOR_VERSION;
//	return true;
//}

//bool EraseRowMem(uint8_t *recv_frame, uint8_t *send_frame){
//	//uint16_t def_addr = 0;
//	//def_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
//	//FLASH_Erase(def_addr);
//
//	FLASH_Erase(recv_frame);
//
//	send_frame[0] = 0x02;	// Length of array
//	send_frame[1] = ERASE_ROW_MEM;	
//	return true;
//}

bool ReadFromMem(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t i = 0;
	uint16_t dbyte = 0;
	uint16_t def_addr = 0;
	def_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
	if (recv_frame[2] == 0x80){
		dbyte = FLASH_Read(def_addr);
		send_frame[2] = (uint8_t)((dbyte & 0xFF00) >> 8);
		send_frame[3] = (uint8_t)(dbyte & 0x00FF);
		i++; i++;
	} else {
		for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
			dbyte = FLASH_Read(def_addr);	
			send_frame[i+2] = (uint8_t)((dbyte & 0xFF00) >> 8);
			send_frame[i+3] = (uint8_t)(dbyte & 0x00FF);
			def_addr++;
		}
	}
	send_frame[0] = i + 2;	// Length of array
	send_frame[1] = READ_FROM_MEM;	
	return true;
}

bool WriteToMem(uint8_t *recv_frame, uint8_t *send_frame){
	bool result;
	uint16_t flash_addr;

	// Config-space writes (addrH==0x80) go straight through; they are not app
	// blocks and must not pass through the reset/vector relocation logic.
	if (recv_frame[2] == 0x80){
		result = FLASH_Write(recv_frame);
	} else {
		flash_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
		result = WriteAppBlock(flash_addr, &recv_frame[4]);
	}

	if (result) {
		send_frame[0] = 0x02;	// Length of array
		send_frame[1] = SUCCESS_CODE;
	}
	return result;
}

// Write one 32-word application block, applying the bootloader's memory rules.
// This is the single place where the reset/vector relocation and self-protection
// live, shared by both flashing paths (UART WriteToMem and autonomous ExtUpgrade).
//
//   flash_addr : destination WORD address (must be 32-word aligned)
//   data64     : 64 bytes = 32 words, big-endian (H,L per word)
//
// Rules:
//   * reset row (0x0000): keep the existing "GOTO bootloader" in words 0-3,
//     take the app data for words 0x0004-0x001F from data64, and relocate the
//     app's own reset vector (data64 words 0-3) into the flags row at 0x3FFC.
//   * bootloader code (0x3800-0x3FDF): refuse (self-protection).
//   * anything else: write data64 as-is.
// Returns true on success, false if the block was rejected/failed.
bool WriteAppBlock(uint16_t flash_addr, uint8_t *data64){
	uint8_t frame[MAX_BLOCK_BYTES_SIZE + 4];
	uint8_t i = 0;
	uint16_t w = 0;

	// Protect the bootloader's own code region.
	if (flash_addr >= BL_CODE_START && flash_addr <= BL_CODE_END){
		return false;
	}

	frame[2] = (uint8_t)((flash_addr & 0xFF00) >> 8);
	frame[3] = (uint8_t)(flash_addr & 0x00FF);

	if (flash_addr == RESET_ROW_ADDR){
		// Words 0-3: keep the current "GOTO bootloader" that is already in flash.
		for (i = 0; i < RESET_VECTOR_NWORDS; i++){
			w = FLASH_Read(RESET_ROW_ADDR + i);
			frame[4 + i*2]     = (uint8_t)((w & 0xFF00) >> 8);
			frame[4 + i*2 + 1] = (uint8_t)(w & 0x00FF);
		}
		// Words 4..31: application data from the incoming block.
		for (i = RESET_VECTOR_NWORDS*2; i < MAX_BLOCK_BYTES_SIZE; i++){
			frame[4 + i] = data64[i];
		}
		if (!FLASH_Write(frame)) return false;

		// Relocate the app's own reset vector (data64 words 0-3) to 0x3FFC,
		// inside the flags row (read-modify-write to keep the flags intact).
		return WriteAppResetVector(data64);
	}

	// Ordinary application block: write as-is.
	for (i = 0; i < MAX_BLOCK_BYTES_SIZE; i++){
		frame[4 + i] = data64[i];
	}
	return FLASH_Write(frame);
}

void StartApp(void){
    IO_RE0_SetLow();
    IO_RE1_SetLow();
    asm ("pagesel " str(APP_RESET_VECTOR));
    asm ("goto " str(APP_RESET_VECTOR));

}

void ExtUpgrade(void){
	uint16_t i = 0, k = 0, addr = BLFlags.StartAddrExtUpgrade;
	uint8_t addr_to_read_serial_mem[4];
	uint8_t buf_from_serial_mem[MAX_BLOCK_BYTES_SIZE + 2];
    uint8_t data_to_flash[MAX_BLOCK_BYTES_SIZE + 4];

    ClearArray(data_to_flash, MAX_BLOCK_BYTES_SIZE + 4);
    
	for (i = 0; i != BLFlags.NumBlocksExtUpgrade; i++){

		addr_to_read_serial_mem[2] = (uint8_t)((addr & 0xFF00) >> 8);
		addr_to_read_serial_mem[3] = (uint8_t)((addr & 0x00FF));


		ReadFromSerialEEPROM(addr_to_read_serial_mem, buf_from_serial_mem);

		data_to_flash[2] = buf_from_serial_mem[2];
		data_to_flash[3] = buf_from_serial_mem[3];

		// To read second time with shift for 1 byte, to append in format > addr+data
		addr_to_read_serial_mem[2] = (uint8_t)((addr+2 & 0xFF00) >> 8);
		addr_to_read_serial_mem[3] = (uint8_t)((addr+2 & 0x00FF));

		ReadFromSerialEEPROM(addr_to_read_serial_mem, buf_from_serial_mem);

		for (k = 0; k != MAX_BLOCK_BYTES_SIZE; k++){
			data_to_flash[k + 4] = buf_from_serial_mem[k + 2]; // Start from2 to cut system info
		}

		//UART_dataWrite(data_to_flash, MAX_BLOCK_BYTES_SIZE + 4);
		FLASH_Write(data_to_flash);

		addr += EEPROM_25LC512_BLOCK_SIZE_BYTES;


	}

	return;

}
