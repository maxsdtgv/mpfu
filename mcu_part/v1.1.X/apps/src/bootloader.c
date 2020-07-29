#include "../api/bootloader.h"



void clearArray(uint8_t *array){
	uint8_t i = 0;
    for (i = 0; i < BL_MAX_SEND_DATA; i++){array[i]=0x00;} // Clear array to send
}

bool defineError(uint8_t *send_frame){
	send_frame[0] = 0x02;     		// Number of bytes to send in response
	send_frame[1] = ERROR_CODE;     // Error code
	return true;
}

bool pingRequest(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t i = 0;
	for (i = 0; i < recv_frame[0]; i++){
		send_frame[i] = recv_frame[i];
	}
	send_frame[1] = PING_REPLY;
	return true;
}