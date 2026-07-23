#include "serial.h"

uint8_t UART_dataWrite(uint8_t *data_ptr, uint8_t size){
    uint8_t i = 0;
    if (!EUSART_is_tx_done()){
    	__delay_us(DELAY_IF_UART_BUSY_us);
    }

    if (EUSART_is_tx_ready()){
        EUSART_Write(PREAM_SEND_TO_HOST);
        for (i = 0; i != size; i++){
            EUSART_Write(data_ptr[i]);
        }   
//        EUSART_Write(LINE_FEED);
    }
    return i;
}

uint8_t UART_byteRead(void){
        return EUSART_Read();
}

bool UART_preamFound(void){
    uint8_t byte = 0;
    if (EUSART_is_rx_ready()){
        byte = EUSART_Read();
        if (byte == PREAM_RECV_FROM_HOST){
            return true;
        }
    }
    return false;
}
