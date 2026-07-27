/*
 * app_bootentry — "enter the bootloader on request" helper for APPLICATIONS.
 *
 * This is the APPLICATION side of the MPFU update flow, and a reference
 * implementation that real firmware can copy. It lets a running application be
 * put into the bootloader over UART, so no physical RB0 button press is needed:
 *
 *     host (mpfu --goto-bl)  ->  application  ->  sets IsBLStart, resets
 *                                             ->  bootloader serves the update
 *
 * The application owns this code: the bootloader will NOT set the flag for a
 * running app, and a plain WRITE_TO_MEM to the flags row is refused by design.
 * The app therefore writes the flags row with its own flash routine (below).
 *
 * If nothing then talks to the bootloader, its inactivity watchdog (~256 s)
 * resets the MCU and this application starts again — see docs/MEMORY.md.
 *
 * Usage in an application:
 *     AppBootEntry_Init();          // once, after your own clock/pin setup
 *     for (;;) { AppBootEntry_Poll();  ...your work... }
 *
 * Poll() is non-blocking: it returns immediately unless a byte is waiting, and
 * never returns at all if a valid "enter bootloader" request arrives (it resets).
 */
#ifndef APP_BOOTENTRY_H
#define APP_BOOTENTRY_H

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

/* --- Frame protocol (same shape as the bootloader's; see docs/PROTOCOL.md) --- */
#define APP_PREAM_FROM_HOST   0x55
#define APP_PREAM_TO_HOST     0xAA
#define APP_SUCCESS_CODE      0xEE
#define APP_ERROR_CODE        0xFF

/* Command handled by the APPLICATION (not by the bootloader). */
#define APP_ENTER_BOOTLOADER  0x1A

/* Bootloader flags row and the IsBLStart flag inside it (see docs/MEMORY.md).
 * Only the LOW byte of a flag word is significant; 0x00 means "set". */
#define APP_FLAGS_ROW         0x3FC0
#define APP_FLAG_OFF_IS_BL_START 0
#define APP_FLAG_SET_BYTE     0x00
#define APP_ROW_WORDS         32

/* Initialise the EUSART (115200 8N1 on RC6/RC7) used to receive the request. */
void AppBootEntry_Init(void);

/* Poll the UART once. Handles an "enter bootloader" request (ACK, set flag,
 * reset — does not return in that case). Safe to call from a tight main loop. */
void AppBootEntry_Poll(void);

/* Set IsBLStart and reset into the bootloader immediately (does not return).
 * Exposed so an application can trigger the same thing from its own logic. */
void AppBootEntry_EnterBootloader(void);

#endif /* APP_BOOTENTRY_H */
