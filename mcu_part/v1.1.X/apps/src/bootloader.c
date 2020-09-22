#include "../api/bootloader.h"

bool KeyBLRequired(void){
	if (IO_RB0_GetValue() == 1) { 
		return true;
	} else {
		return false;
	}
}

void ClearArray(uint8_t *array, uint8_t len){
	uint8_t i = 0;
    for (i = 0; i != len; i++){array[i]=0x00;} // Clear array to send
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
	result = FLASH_Write(recv_frame);
	if (result) {
	send_frame[0] = 0x02;	// Length of array
	send_frame[1] = WRITE_TO_MEM;			
	}
	return result;
}

void StartApp(void){
    //STKPTR = 0x1F;
    //asm ("psect intentry,global,class=CODE,delta=2");  
    IO_RE0_SetLow();
    while(1){}
    asm ("pagesel " str(RESET_VECTOR_APP));
    asm ("goto " str(RESET_VECTOR_APP));
}

void ExtUpgrade(void){
	uint16_t i = 0, k = 0, addr = BLFlags.NumBlocksExtUpgrade;
	uint8_t command_to_read[4];
	uint8_t buf_from_serial_mem[MAX_BLOCK_BYTES_SIZE + 2];
    uint8_t data_to_flash[MAX_BLOCK_BYTES_SIZE + 4];

    ClearArray(data_to_flash, MAX_BLOCK_BYTES_SIZE + 4);
    
	for (i = 0; i != BLFlags.NumBlocksExtUpgrade; i++){

		command_to_read[2] = (uint8_t)((addr & 0xFF00) >> 8);
		command_to_read[3] = (uint8_t)((addr & 0x00FF));


		ReadFromSerialEEPROM(command_to_read, buf_from_serial_mem);

		data_to_flash[2] = buf_from_serial_mem[2];
		data_to_flash[3] = buf_from_serial_mem[3];

		addr++;

		command_to_read[2] = (uint8_t)((addr & 0xFF00) >> 8);
		command_to_read[3] = (uint8_t)((addr & 0x00FF));

		ReadFromSerialEEPROM(command_to_read, buf_from_serial_mem);

		for (k = 0; k != MAX_BLOCK_BYTES_SIZE; k++){
			data_to_flash[k + 4] = buf_from_serial_mem[k + 2];
		}



		UART_dataWrite(data_to_flash, 0x44);
		//FLASH_Write(data_to_flash);

	}



}
