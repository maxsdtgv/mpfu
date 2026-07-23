# MPFU Hardware Configuration

Target MCU: **PIC16F1789** (enhanced mid-range, 16K words flash).

These values reflect the actual bootloader configuration (verified on hardware,
2026-07-23).

## Clock

- Internal oscillator (**HFINTOSC**), `FOSC = INTOSC`.
- Running at **16 MHz** (`OSCCON = 0x7A`: IRCF=1111 → 16 MHz, SCS=internal;
  `_XTAL_FREQ = 16000000`).
- Config bit `PLLEN = ON` is set, but the bootloader runs the core at 16 MHz.

## Configuration bits

```
FOSC=INTOSC  WDTE=OFF   PWRTE=OFF  MCLRE=ON   CP=OFF    CPD=OFF
BOREN=ON     CLKOUTEN=OFF  IESO=ON  FCMEN=ON  WRT=OFF   VCAPEN=OFF
PLLEN=ON     STVREN=ON  BORV=LO    LPBOR=OFF  LVP=ON
```

Notable points:

- **WDTE = OFF** — the watchdog is disabled. (An earlier note claimed a ~135 s
  WDT; that was wrong.)
- **STVREN = ON** — a stack overflow/underflow causes a reset. This is why, with
  an empty application vector, the chip visibly loops through resets: `StartApp`
  jumps to blank flash, runs garbage, and eventually trips a stack reset.
- **LVP = ON** — low-voltage programming enabled.
- **WRT = OFF** — no flash self-write protection (needed so the bootloader can
  write flash).

## Peripherals / pins

| Function        | Pins                          | Settings                          |
|-----------------|-------------------------------|-----------------------------------|
| UART (EUSART)   | RC6 = TX, RC7 = RX            | **115200 baud, 8N1**              |
| SPI (MSSP)      | RC3 = SCK, RC4 = SDI, RC5 = SDO | Master, ~250 kHz (Fosc/64)      |
| Ext. EEPROM CS  | RA0                            | active low (25LC512)              |
| "Enter BL" key  | RB0                            | read high at startup → stay in BL |
| Status LEDs     | RE0, RE1, RE2                  | see below                         |

### UART baud detail

`SP1BRGL = 0x22 (34)`, `BRGH = 1`, `BRG16 = 1` → baud = Fosc / (4·(34+1)) =
16e6/140 = **114286**, i.e. 115200 within 0.8% — use **115200** on the host.

### LED indicators (bootloader)

- **RE0** — ON while the bootloader is resident and waiting for commands.
- **RE1** — pulses while a UART frame is being received/processed.
- **RE2** — not used by the bootloader (the sample app toggles it).

## External serial EEPROM

- **Microchip 25LC512** (SPI), used by the autonomous `ExtUpgrade` path: a block
  of firmware can be staged in the EEPROM and programmed into flash on the next
  reset when `IsExtUpgrade` is set. Commands `0x12`/`0x14` read/write it.

## Programmer

- Bench programmer: **PICkit 3** (a clone, enumerates as `04d8:900a`).
- Flashed via `ipecmd` from **MPLAB X v5.35** (later MPLAB X dropped PICkit 3).
  See docs/BUILD_AND_FLASH.md — `flash.sh` handles the details.
