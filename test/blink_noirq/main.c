/*
 * blink_noirq — minimal PIC16F1789 blinker with NO interrupts.
 *
 * Purpose: a test-fixture APPLICATION compiled COMPLETELY NORMALLY
 * (reset at 0x0000, no ISR). Toggles RE0/RE1/RE2 in a busy-delay loop.
 * Counterpart to ../blink_irq (which uses a Timer0 ISR at 0x0004); together
 * they check that mpfu's row-0 / reset-vector relocation works for apps both
 * with and without an interrupt handler.
 */
#include <xc.h>

// ---- Configuration bits (mirror blink_irq) ----
#pragma config FOSC = INTOSC, WDTE = OFF, PWRTE = OFF, MCLRE = ON
#pragma config CP = OFF, CPD = OFF, BOREN = ON, CLKOUTEN = OFF
#pragma config IESO = ON, FCMEN = ON
#pragma config WRT = OFF, VCAPEN = OFF, PLLEN = OFF, STVREN = ON
#pragma config BORV = LO, LPBOR = OFF, LVP = ON

#define _XTAL_FREQ 16000000

void main(void)
{
    // 16 MHz HFINTOSC
    OSCCON = 0x78;

    // RE0/RE1/RE2 as outputs
    TRISE = 0x00;
    LATE  = 0x00;

    while (1) {
        __delay_ms(500);
        LATEbits.LATE0 ^= 1;
        LATEbits.LATE1 ^= 1;
        LATEbits.LATE2 ^= 1;
    }
}
