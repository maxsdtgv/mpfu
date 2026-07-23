# MPFU Release 1.1

Pre-built artifacts for **PIC16F1789**.

| File           | Description                                            |
|----------------|--------------------------------------------------------|
| `mcu_1.1.hex`  | Bootloader firmware (flash with a PICkit 3 via ipecmd) |
| `mpfu_1.1`     | Linux host uploader (x86-64)                           |

> Target device: **PIC16F1789 only.** The firmware is built with `-mcpu=16f1789`
> and a fixed memory map; it will not work on other parts without rebuilding.

## What works in 1.1

- **UART application update** — stream a normally-compiled application into
  flash over the serial link, with per-block read-back verify.
- **Autonomous EEPROM update (ExtUpgrade)** — a firmware image staged in an
  external SPI EEPROM (25LC512) is programmed into flash on the next reset when
  the `IsExtUpgrade` flag is set (basis for over-the-air updates).
- **Raw EEPROM write** (`-e`) and **offline image generation** (`-g`).
- **Bootloader self-protection** — preserves its jump at `0x0000-0x0003`,
  refuses writes into its code region `0x3800-0x3FDF`, relocates the app reset
  vector to `0x3FFC`. Same rules for both update paths.
- Bootloader at `0x3800-0x3FDF`; application region `0x0000-0x37FF`.
- Optional Fletcher-16 image integrity check is **disabled** in this build
  (`USE_FLETCHER` off) to save flash.

Bootloader size: 1946 of 16384 words (~12%).

## Hardware

- MCU: PIC16F1789, internal osc 16 MHz, WDT off.
- UART: RC6 (TX) / RC7 (RX), **115200 8N1**.
- External EEPROM: 25LC512 on SPI (RC3/RC4/RC5, CS on RA0).
- "Enter bootloader" button: RB0 (held at reset).
- Status LEDs: RE0 (in bootloader), RE1 (frame activity).

## How to use

### 1. Flash the bootloader (one time, with a PICkit 3)

Requires MPLAB X v5.35's `ipecmd` running on its bundled Java 8:

```bash
J8=/opt/microchip/mplabx/v5.35/sys/java/jre1.8.0_181/bin/java
IPE=/opt/microchip/mplabx/v5.35/mplab_platform/mplab_ipe
"$J8" -jar "$IPE/ipecmd.jar" -TPPK3 -P16F1789 -M -Fmcu_1.1.hex -OL
```

(Or use `mcu_part/flash.sh` from the source tree.)

### 2. Flash an application over UART

Enter the bootloader (hold **RB0** at reset until **RE0** lights), then:

```bash
./mpfu_1.1 -D /dev/ttyUSB0 -b 115200 -f your_app.hex -s
```

The application must be compiled normally (reset at `0x0000`); the bootloader
handles all vector relocation.

### 3. Autonomous update via EEPROM (optional)

```bash
# Build an EEPROM image offline, then write it into the EEPROM:
./mpfu_1.1 -g your_app.hex app.img
./mpfu_1.1 -D /dev/ttyUSB0 -b 115200 -e app.img
# Set IsExtUpgrade (flags row 0x3FE1 low byte = 0x00) and EXT_ADDR (0x3FE2/3),
# then reset — the bootloader programs flash from EEPROM and starts the app.
```

See the top-level `docs/` for protocol, memory map and full build instructions.
