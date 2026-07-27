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
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.5
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
    bool    extUpgradeFailed = false;   // bad EEPROM image -> stay in the BL

    ClearArray(recv_frame, BL_MAX_RECV_DATA);
    ClearArray(send_frame, BL_MAX_SEND_DATA);

    //TRISx registers

    //TRISA = 0x00;
    //TRISB = 0x03;   // PortB - 0,1,2 for input
    //TRISE = 0x00;

    //LATA = 0xFF;    
    //LATE = 0x00;

    ReadBootloaderFlags();

    if (BLFlags.IsExtUpgrade){
        // Clear the request BEFORE attempting the upgrade, and persist it now.
        // If the upgrade itself hangs (dead SPI bus, EEPROM not answering) the
        // watchdog below resets us; with the flag already cleared we come back
        // as a normal boot instead of retrying the same hang forever.
        BLFlags.IsExtUpgrade = false;       // one-shot request
        WriteBootloaderFlags();

        // Arm the watchdog for the upgrade itself, not just for the UART loop:
        // an autonomous OTA update runs with no host attached, so a stuck
        // transfer must not be able to strand the unit. ExtUpgrade() pets it
        // as it goes (per block), so only a real hang trips it.
        CLRWDT();
        WDTCON = WDT_BL_TIMEOUT_CONF;

        ExtUpgrade();
        WriteBootloaderFlags();             // persist the resulting status code

        // Launch the app only if the upgrade succeeded. On a bad image the
        // flash was left untouched, so we deliberately stay in the bootloader
        // (RE0 on) to let a host intervene; extUpgradeFailed keeps the plain
        // "no RB0, no IsBLStart -> start the app" check below from firing.
        // If no host shows up, the inactivity watchdog resets us after ~256 s
        // and the previous (still intact) application boots.
        if (BLFlags.StatusCodeExtUpgrade == EXTUP_STATUS_OK){
            StartApp();
        }
        extUpgradeFailed = true;
    }

    if (!extUpgradeFailed && !KeyBLRequired() && !BLFlags.IsBLStart){
        StartApp();
    }



    BLFlags.IsBLStart = false;  // Clear BL start flag

    WriteBootloaderFlags();  

    IO_RE0_SetHigh();

    // Arm the inactivity dead-man. From here the watchdog runs and is petted
    // ONLY when a frame actually arrives (CLRWDT below), so ~256 s without any
    // host traffic resets the MCU. IsBLStart was already cleared above, so the
    // application boots on that reset and a field unit can never be stranded
    // in the bootloader. StartApp() disables the WDT again. See bootloader.h.
    CLRWDT();                       // start counting from zero
    WDTCON = WDT_BL_TIMEOUT_CONF;   // WDTPS = 1:8388608 (~256 s), SWDTEN = 1

    while (1)
    {

        if (UART_preamFound())                          // Try to found PREAM_RECV_FROM_HOST in the byte [0] of frame header
            {
            IO_RE1_SetHigh();
            CLRWDT();                                   // Clear WDT;

            recv_frame[0] = UART_byteRead();            // Read second byte with amount of next expected bytes
            if ((recv_frame[0] > BL_MAX_RECV_DATA) || (recv_frame[0] == 0x00 ))
                {
                recv_frame[1] = 0x00;               // Define 0x00 to choose default in switch case
                }
            else{
                for (i = 1; i != recv_frame[0]; i++)
                    {    // Receive array with data
                    recv_frame[i] = UART_byteRead();
                    }
        
                }
        // Exec switch to handle requestf from host
            
            switch (recv_frame[1])              // Analize first byte of the received array
                {
//                case PING_REQUEST:            // 0x01 - Ping request.
//                    processing_status = pingRequest(recv_frame, send_frame); // Send_frame will be filled after execution
//                    break;
                
                case READ_FROM_MEM:             // 0x02 - Read from mem with any address requested.
                    processing_status = ReadFromMem(recv_frame, send_frame); // Send_frame will be filled after execution
                    break;

                //case ERASE_ROW_MEM:           // 0x03 - Erase row mem.
                //    processing_status = EraseRowMem(recv_frame, send_frame); // Send_frame will be filled after execution
                //    break;

                case WRITE_TO_MEM:              // 0x04 - Write to mem.
                    processing_status = WriteToMem(recv_frame, send_frame); // Send_frame will be filled after execution
                    break;

                case SET_EXT_UPGRADE:           // 0x16 - Arm ExtUpgrade + reset (does not return)
                    processing_status = SetExtUpgrade(recv_frame, send_frame);
                    break;

                case READ_FROM_SERIAL_EEPROM:   // 0x12 - Read from external serial eeprom
                    processing_status = ReadFromSerialEEPROM(recv_frame, send_frame); // Send_frame will be filled after execution
                    break;

                case WRITE_TO_SERIAL_EEPROM:    // 0x14 - Write to external serial eeprom
                    processing_status = WriteToSerialEEPROM(recv_frame, send_frame); // Send_frame will be filled after execution
                    break;

                case START_APPLICATION:         // 0x0F - Exit Bootloader. GOTO to RVA (see memory structure)
                    StartApp();
                    break;

                default:
                    processing_status = DefineError(send_frame);
                }

        // Send response if processing status is TRUE
            if (!((processing_status == true) && (send_frame[0] != 0x00)))
                {
                DefineError(send_frame);
                } 

                UART_dataWrite(send_frame, send_frame[0]);          // Send frame to uart
                processing_status = false;                          // Finish processing
            ClearArray(recv_frame, BL_MAX_RECV_DATA);
            ClearArray(send_frame, BL_MAX_SEND_DATA);
                            
            IO_RE1_SetLow();

            }
    }
}
/**
 End of File
*/