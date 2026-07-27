/*
 * blink_irq — minimal PIC16F1789 blinker driven by a Timer0 interrupt.
 *
 * Purpose: a test-fixture APPLICATION that is compiled COMPLETELY NORMALLY
 * (reset at 0x0000, ISR at 0x0004, code starting in row 0). Used to exercise
 * the row-0 / vector handling with an interrupt handler present, and to prove
 * the application's ISR keeps running at full speed after an update.
 *
 * It also serves the "enter bootloader" request over UART (see ../common), so
 * `mpfu --goto-bl` can update it without touching the RB0 button.
 *
 * Build: this is a self-contained source (no MCC). See build.sh in this dir.
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

volatile unsigned int tick = 0;

// Interrupt service routine — XC8 places this at the interrupt vector (0x0004).
void __interrupt() isr(void)
{
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;   // clear Timer0 overflow flag
        tick++;
        if (tick >= 500) {       // toggle roughly ~1-2 Hz depending on prescale
            tick = 0;
            LATEbits.LATE0 ^= 1;
            LATEbits.LATE1 ^= 1;
            LATEbits.LATE2 ^= 1;
        }
    }
}

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

    AppBootEntry_Init();         // UART 115200 on RC6/RC7

    // Timer0: internal clock, prescaler 1:256
    OPTION_REG = 0x07;           // PSA=0, prescaler 111 = 1:256, T0CS=0

    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;       // enable Timer0 interrupt
    INTCONbits.GIE    = 1;       // global interrupts on

    while (1) {
        // Blinking happens in the ISR; the main loop only listens for the
        // host's "enter bootloader" request.
        AppBootEntry_Poll();     // may reset into the bootloader
    }
}
