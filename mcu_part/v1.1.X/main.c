/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.4
        Device            :  PIC16F1789
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "main.h"

/*
                         Main application
 */
void main(void)
{
 
    // initialize the device
    SYSTEM_Initialize();

    uint8_t recv_frame[BL_MAX_RECV_DATA];
    uint8_t send_frame[BL_MAX_SEND_DATA];
    uint8_t i = 0;
    uint8_t byte = 0;
    bool    processing_status = false;

    ClearArray(recv_frame);
    ClearArray(send_frame);

    UART_dataWrite(send_frame, 0x00); // Will send empty line, just PREAM + LF  


    while (1)
    {

// Temporary solution to clear WDT.
CLRWDT();                                   // Clear WDT;

        if (UART_preamFound())                          // Try to found PREAM_RECV_FROM_HOST in the byte [0] of frame header
            {
            CLRWDT();                                   // Clear WDT;
            recv_frame[0] = UART_byteRead();            // Read second byte with amount of next expected bytes
            if ((recv_frame[0] > BL_MAX_RECV_DATA) || (recv_frame[0] == 0x00 ))
                {
                recv_frame[1] = 0x00;               // Define 0x00 to choose default in switch case
                }
            else{
                for (i = 1; i < recv_frame[0]; i++)
                    {    // Receive array with data
                    recv_frame[i] = UART_byteRead();
                    }
        
                }
        // Exec switch to handle requestf from host
            
            switch (recv_frame[1])             // Analize first byte of the received array
                {
//                case PING_REQUEST:              // 0x01 - Ping request.
//                    processing_status = pingRequest(recv_frame, send_frame); // Send_frame will be filled after execution
//                    break;
                
                case READ_FROM_MEM:              // 0x02 - Read from mem with any address requested.
                    processing_status = ReadFromMem(recv_frame, send_frame); // Send_frame will be filled after execution
                    break;

                default:
                    processing_status = DefineError(send_frame);
                }

        // Send response if processing status is TRUE
            if ((processing_status == true) && (send_frame[0] != 0x00))
                {
                UART_dataWrite(send_frame, send_frame[0]);          // Send frame to uart
                processing_status = false;            // Finish processing
                }

            ClearArray(recv_frame);
            ClearArray(send_frame);
            }
    }
}
/**
 End of File
*/