#include "../api/bootloader.h"

void ClearArray(uint8_t *array){
	uint8_t i = 0;
    for (i = 0; i < BL_MAX_SEND_DATA; i++){array[i]=0x00;} // Clear array to send
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

		for (i = 0; i < MAX_BLOCK_SIZE+MAX_BLOCK_SIZE; i += 2){
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
