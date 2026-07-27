#include "app_bootentry.h"

/* ---------------------------------------------------------------------------
 * UART (EUSART) — mirrors the bootloader's settings so the same host tool and
 * baud rate work in both: 115200 8N1 on RC6 (TX) / RC7 (RX) at Fosc = 16 MHz.
 * SP1BRGL = 34 with BRGH = 1 and BRG16 = 1 -> 16e6/(4*(34+1)) = 114286 (~0.8% off).
 * --------------------------------------------------------------------------- */
void AppBootEntry_Init(void)
{
    /* RC6 = TX (output), RC7 = RX (input); both must be digital. */
    ANSELCbits.ANSC7 = 0;
    TRISCbits.TRISC6 = 0;
    TRISCbits.TRISC7 = 1;

    APFCON1 = 0x00;          /* default pin mapping (TX/RX on RC6/RC7) */
    APFCON2 = 0x00;

    BAUD1CON = 0x08;         /* BRG16 = 1 */
    RC1STA   = 0x90;         /* SPEN = 1, CREN = 1 */
    TX1STA   = 0x24;         /* TXEN = 1, BRGH = 1 */
    SP1BRGL  = 0x22;         /* 34 */
    SP1BRGH  = 0x00;
}

static void txByte(uint8_t b)
{
    while (!PIR1bits.TXIF) { }
    TX1REG = b;
}

/* Receive one byte with a bounded wait, so a truncated frame cannot hang the
 * application. Returns false on timeout. */
static bool rxByteTimeout(uint8_t *out)
{
    uint16_t guard = 0;
    while (!PIR1bits.RCIF) {
        if (++guard == 0) {
            return false;              /* ~65k polls: give up, stay in the app */
        }
    }
    if (RC1STAbits.OERR) {             /* overrun: restart the receiver */
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
    }
    *out = RC1REG;
    return true;
}

/* ---------------------------------------------------------------------------
 * Flash: read-modify-write of the bootloader flags row.
 *
 * The PIC16F1 program-memory write engine latches a whole 32-word row, so we
 * read the row, patch one byte, and write it all back. Interrupts are disabled
 * across the operation (the CPU stalls during the erase/write anyway, and the
 * fixture with an ISR must not take an interrupt mid-sequence).
 * --------------------------------------------------------------------------- */
static uint16_t flashReadWord(uint16_t addr)
{
    EEADRH = (uint8_t)((addr >> 8) & 0xFF);
    EEADRL = (uint8_t)(addr & 0xFF);
    EECON1bits.CFGS  = 0;
    EECON1bits.EEPGD = 1;
    EECON1bits.FREE  = 0;
    EECON1bits.RD    = 1;
    NOP();
    NOP();
    EECON1bits.EEPGD = 0;
    return (uint16_t)(((uint16_t)EEDATH << 8) | EEDATL);
}

static void flashUnlockWrite(void)
{
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;
    NOP();
    NOP();
}

/* Write 32 words to the row starting at rowAddr (must be 0x20-aligned). */
static void flashWriteRow(uint16_t rowAddr, const uint16_t *words)
{
    uint8_t  i;
    uint16_t addr = rowAddr;
    uint8_t  gie  = (uint8_t)INTCONbits.GIE;

    INTCONbits.GIE = 0;

    EEADRH = (uint8_t)((rowAddr >> 8) & 0xFF);
    EEADRL = (uint8_t)(rowAddr & 0xFF);

    EECON1bits.EEPGD = 1;
    EECON1bits.CFGS  = 0;
    EECON1bits.WREN  = 1;
    EECON1bits.WRERR = 0;

    /* Erase the row first. */
    EECON1bits.FREE = 1;
    flashUnlockWrite();
    EECON1bits.FREE = 0;

    /* Load the 32 write latches, then commit with the last word. */
    EECON1bits.LWLO = 1;
    for (i = 0; i < APP_ROW_WORDS; i++) {
        EEADRH = (uint8_t)((addr >> 8) & 0xFF);
        EEADRL = (uint8_t)(addr & 0xFF);
        EEDATH = (uint8_t)((words[i] >> 8) & 0x3F);
        EEDATL = (uint8_t)(words[i] & 0xFF);
        if (i == (APP_ROW_WORDS - 1)) {
            EECON1bits.LWLO = 0;       /* last word: this triggers the write */
        }
        flashUnlockWrite();
        addr++;
    }

    EECON1bits.WREN  = 0;
    EECON1bits.EEPGD = 0;
    INTCONbits.GIE   = gie;
}

void AppBootEntry_EnterBootloader(void)
{
    uint16_t row[APP_ROW_WORDS];
    uint8_t  i;

    /* Preserve every other flag / word in the row. */
    for (i = 0; i < APP_ROW_WORDS; i++) {
        row[i] = flashReadWord((uint16_t)(APP_FLAGS_ROW + i));
    }

    /* Set IsBLStart: low byte = 0x00 ("flag is set"), keep the high bits. */
    row[APP_FLAG_OFF_IS_BL_START] =
        (uint16_t)((row[APP_FLAG_OFF_IS_BL_START] & 0x3F00) | APP_FLAG_SET_BYTE);

    flashWriteRow(APP_FLAGS_ROW, row);

    /* Reset: the bootloader sees IsBLStart and stays in its command loop. */
    asm("reset");
    for (;;) { }                       /* not reached */
}

void AppBootEntry_Poll(void)
{
    uint8_t b, len, cmd;

    if (!PIR1bits.RCIF) {
        return;                        /* nothing pending: cheap early exit */
    }

    if (RC1STAbits.OERR) {
        /* Overrun: the FIFO is stuck and its contents are unreliable. Flush the
         * receiver and resynchronise on the next frame instead of acting on a
         * byte we cannot trust. */
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
        return;
    }
    b = RC1REG;

    if (b != APP_PREAM_FROM_HOST) {
        return;                        /* not a frame start: resynchronise */
    }
    if (!rxByteTimeout(&len)) return;
    if (!rxByteTimeout(&cmd)) return;

    /* Drain any remaining payload bytes so we stay in frame sync. LEN counts
     * from the LEN byte itself, so payload = LEN - 2 (LEN and CMD). */
    while (len > 2) {
        uint8_t junk;
        if (!rxByteTimeout(&junk)) return;
        len--;
    }

    if (cmd == APP_ENTER_BOOTLOADER) {
        /* Acknowledge BEFORE resetting, so the host knows the request landed. */
        txByte(APP_PREAM_TO_HOST);
        txByte(0x02);
        txByte(APP_SUCCESS_CODE);
        while (!TX1STAbits.TRMT) { }   /* let the last byte leave the shifter */
        AppBootEntry_EnterBootloader();/* does not return */
    }

    /* Any other command: the application does not implement the bootloader
     * protocol, so report an error rather than staying silent. */
    txByte(APP_PREAM_TO_HOST);
    txByte(0x02);
    txByte(APP_ERROR_CODE);
}
