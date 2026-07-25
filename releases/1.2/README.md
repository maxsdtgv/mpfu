# MPFU Release 1.2

Pre-built artifacts for **PIC16F1789**.

| File            | Description                                            |
|-----------------|--------------------------------------------------------|
| `mcu_1.2.hex`   | Bootloader firmware (flash with a PICkit 3 via ipecmd) |
| `mpfu_1.2`      | Linux host uploader (x86-64)                           |
| `configs/`      | Device profiles (needed by the host at runtime)        |

> Target device: **PIC16F1789 only.** The firmware is built with a fixed memory
> map for this part; it will not work on other chips without rebuilding.

## What's new in 1.2 (vs 1.1)

- **Fixed the reset-vector bug**: applications *without* an interrupt handler
  (whose reset vector trampolines through `0x0002`) now flash and run correctly.
  1.1 only worked for apps with an ISR.
- **Intelligence moved to the host**: the host resolves the application entry
  point and builds the firmware image; the bootloader is a small, chip-specific
  writer. Platform details live in **device profiles** (`-c 16f1789`).
- **Unified image format v2** for both the UART and EEPROM paths.
- **`SET_EXT_UPGRADE` command / `-u`**: arm the autonomous EEPROM upgrade and
  reset from the host in one step (`-e img -u 0`).
- **Safer OTA**: a bad EEPROM image is rejected and the app is *not* launched;
  the unit stays recoverable over UART.
- New memory map: bootloader `0x3800-0x3FBF`, flags row `0x3FC0`, app-vector row
  `0x3FE0`.

> **Incompatible with 1.1.** The image format and flash layout changed. Reflash
> the 1.2 bootloader with a programmer (a full erase); do not mix 1.1 and 1.2
> artifacts.

## What works in 1.2 (verified on real hardware)

- UART application update (apps with **and without** an ISR), per-block verify.
- Autonomous EEPROM update (ExtUpgrade) for both app types.
- Bootloader self-protection: preserves its trampoline at `0x0000-0x0003`,
  refuses writes into its code region and flags row.
- Optional bootloader checks behind feature flags (`USE_DEVICE_ID_CHECK`,
  `USE_VERSION_CHECK`, `USE_FLETCHER`) — off in this build to save flash.

Bootloader size: 1954 of 16384 words (~12%).

## Hardware

- MCU: PIC16F1789, internal osc 16 MHz, WDT off. Device ID `0x302A`.
- UART: RC6 (TX) / RC7 (RX), **115200 8N1**.
- External EEPROM: 25LC512 on SPI (RC3/RC4/RC5, CS on RA0).
- "Enter bootloader" button: RB0 (held at reset). Status LED: RE0 (in bootloader).

## How to use

### 1. Flash the bootloader (one time, with a PICkit 3)

Requires MPLAB X v5.35's `ipecmd` on its bundled Java 8:

```bash
J8=/opt/microchip/mplabx/v5.35/sys/java/jre1.8.0_181/bin/java
IPE=/opt/microchip/mplabx/v5.35/mplab_platform/mplab_ipe
"$J8" -jar "$IPE/ipecmd.jar" -TPPK3 -P16F1789 -M -Fmcu_1.2.hex -OL
```

(Or use `mcu_part/flash.sh` from the source tree.)

### 2. Flash an application over UART

Enter the bootloader (hold **RB0** at reset until **RE0** lights), then:

```bash
./mpfu_1.2 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -f your_app.hex -s
```

The application must be compiled normally (reset at `0x0000`).

### 3. Autonomous update via EEPROM (optional)

```bash
./mpfu_1.2 -c 16f1789 -g your_app.hex app.img          # build image (offline)
./mpfu_1.2 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -e app.img -u 0   # write + arm + reset
```

The bootloader programs flash from EEPROM on reset and starts the app if the
image verified.

To read the EEPROM back for debugging:

```bash
./mpfu_1.2 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -E dump.img          # auto length
./mpfu_1.2 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -E raw.bin 0x0400 256 # raw range
```

> Keep the `configs/` folder next to `mpfu_1.2` (the host loads the profile from
> `configs/<name>.conf`).

See the top-level `docs/` for protocol, memory map and full build instructions.
