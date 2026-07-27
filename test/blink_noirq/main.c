/*
 * blink_noirq — minimal PIC16F1789 blinker with NO interrupts.
 *
 * Purpose: a test-fixture APPLICATION compiled COMPLETELY NORMALLY
 * (reset at 0x0000, no ISR). Toggles RE0/RE1/RE2 in a busy-delay loop.
 * Counterpart to ../blink_irq (which uses a Timer0 ISR at 0x0004); together
 * they check that mpfu's reset-vector resolution works for apps both with and
 * without an interrupt handler. This one is the important case: with no ISR,
 * XC8 puts `start` at 0x0002 and the reset vector becomes a two-step
 * trampoline — the layout that used to break the bootloader.
 *
 * It also serves the "enter bootloader" request over UART (see ../common), so
 * `mpfu --goto-bl` can update it without touching the RB0 button. Note the UART
 * is POLLED in the main loop on purpose — adding an ISR would defeat the whole
 * point of this fixture.
 */
#include <xc.h>
#include "../common/app_bootentry.h"

// ---- Configuration bits (PIC16F1789) ----
// WDTE = SWDTEN matches the bootloader build: the watchdog stays off unless
// software enables it, so this application is not required to service it.
#pragma config FOSC = INTOSC, WDTE = SWDTEN, PWRTE = OFF, MCLRE = ON
#pragma config CP = OFF, CPD = OFF, BOREN = ON, CLKOUTEN = OFF
#pragma config IESO = ON, FCMEN = ON
#pragma config WRT = OFF, VCAPEN = OFF, PLLEN = OFF, STVREN = ON
#pragma config BORV = LO, LPBOR = OFF, LVP = ON

#define _XTAL_FREQ 16000000

void main(void)
{
    // 16 MHz HFINTOSC. MUST match the bootloader (OSCCON = 0x7A, SCS = 1x):
    // the bootloader's config word has PLLEN = ON, and with SCS = 00 the 4x PLL
    // would engage and change Fosc (datasheet 6.2.2.6), breaking the UART baud
    // rate. SCS = 1x selects the internal oscillator block directly and bypasses
    // the PLL. Config bits come from the BOOTLOADER hex, not from the app.
    OSCCON = 0x7A;

    // RE0/RE1/RE2 as outputs
    TRISE = 0x00;
    LATE  = 0x00;

    AppBootEntry_Init();          // UART 115200 on RC6/RC7

    while (1) {
        // Blink with the delay broken into SHORT slices: the UART receiver only
        // has a 2-byte FIFO, and a 3-byte request takes ~0.8 ms at 115200, so
        // the poll interval must stay well under that or the frame is lost to an
        // overrun. 100 us slices give plenty of margin. (No interrupts are used
        // in this fixture on purpose — that is what it exists to test.)
        for (unsigned int n = 0; n < 5000; n++) {   // 5000 * 100 us = 500 ms
            AppBootEntry_Poll();  // may reset into the bootloader
            __delay_us(100);
        }
        LATEbits.LATE0 ^= 1;
        LATEbits.LATE1 ^= 1;
        LATEbits.LATE2 ^= 1;
    }
}
