/* 
 * File:   serial.h
 * Author: mvysochinenko
 *
 * Created on July 29, 2020, 8:26 PM
 */

#ifndef SERIAL_H
#define	SERIAL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../../main.h"

// Parameters definitions

#define DELAY_IF_UART_BUSY_us	300		// Delay on read byte from UART if serial not ready, use once
#define PREAM_RECV_FROM_HOST	0x55	// Value expected to receive in [0] byte to start read the frame
#define PREAM_SEND_TO_HOST		0xAA	// Value will put to [0] byte to send frame to host
#define LINE_FEED 				0x0A	// Line feed

// Function registration

uint8_t UART_dataWrite(uint8_t*, uint8_t);

uint8_t UART_byteRead(void);

bool UART_preamFound(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SERIAL_H */

