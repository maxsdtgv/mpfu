# MPFU Release 1.4

Pre-built artifacts for **PIC16F1789**.

| File            | Description                                            |
|-----------------|--------------------------------------------------------|
| `mcu_1.4.hex`   | Bootloader firmware (flash with a PICkit 3 via ipecmd) |
| `mpfu_1.4`      | Linux host uploader (x86-64)                           |
| `configs/`      | Device profiles (keep next to the binary)              |

> Target device: **PIC16F1789 only.**

## What's new in 1.4 (vs 1.3)

- **The bootloader now refuses firmware built for a different chip.**
  `USE_DEVICE_ID_CHECK` is enabled by default: before an autonomous EEPROM
  upgrade the bootloader compares the image's `device_id` with its own DEVID
  (read from `0x8006`) and, on a mismatch, leaves flash untouched and records
  status `0x03` (`BAD_DEVICE`) in the flags row. This closes a real over-the-air
  risk — an image for another part can no longer be programmed into this one.
  An image can still opt out by carrying `device_id = 0xFFFF` ("any device").
- **Memory map:** the bootloader region grew by two rows to make room for that
  check — bootloader code is now `0x37C0`–`0x3FBF` (2048 words), and the
  application region is `0x0000`–`0x37BF`. The device profile
  (`configs/16f1789.conf`) and `-mrom` were updated to match.

Bootloader size: 2021 of 2048 region words (27 free). Adding `USE_VERSION_CHECK`
(2029) still fits; `USE_FLETCHER` (2247) does not without extending the region
further.

> **Upgrading from 1.3:** reflash the bootloader with a programmer and use the
> `configs/` shipped here — a 1.3 profile still says `bl_code_start = 0x3800` and
> would let the host place application data in `0x37C0`–`0x37FF`, which this
> bootloader refuses. The image format (v2) is unchanged.

## Everything from 1.3 still applies

- **No RB0 button needed:** `mpfu --goto-bl` asks the running application to hand
  over to the bootloader (command `0x1A`, implemented by the application).
- **The bootloader always returns to the application:** a watchdog dead-man
  (~4.3 min, measured 260–270 s — the LFINTOSC is uncalibrated) covers both the
  UART command loop and the autonomous EEPROM upgrade.
- **A failed OTA never runs garbage and never bricks the unit:** a rejected image
  leaves flash untouched, the bootloader waits for a host, and if none comes the
  watchdog restores the previous working application.
- Built with `WDTE = SWDTEN`, so applications need no watchdog handling.

## Verified on real hardware

- Image with a wrong `device_id` (0x3060 vs chip 0x302A): **rejected**, flash
  untouched, status read back from `0x3FC4` = `0x03` (BAD_DEVICE).
- Correct image: accepted and programmed autonomously from EEPROM.
- Corrupt image (bad magic): rejected, previous application restored by the
  watchdog after ~4.5 min.
- UART updates of apps with and without an ISR, with and without the button.

## Quick start

```bash
# 1. Flash the bootloader once with a PICkit 3
J8=/opt/microchip/mplabx/v5.35/sys/java/jre1.8.0_181/bin/java
IPE=/opt/microchip/mplabx/v5.35/mplab_platform/mplab_ipe
"$J8" -jar "$IPE/ipecmd.jar" -TPPK3 -P16F1789 -M -Fmcu_1.4.hex -OL

# 2. First application: hold RB0, power-cycle, then
./mpfu_1.4 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -f your_app.hex -s

# 3. From then on, no button (if the app implements ENTER_BOOTLOADER)
./mpfu_1.4 -D /dev/ttyUSB0 -b 115200 -c 16f1789 --goto-bl -f your_app.hex -s
```

> Writing your own application? Use `OSCCON = 0x7A` (SCS = `1x`), like the
> bootloader — applications inherit the bootloader's configuration words, which
> have `PLLEN = ON`, and with `SCS = 00` the 4x PLL changes Fosc and breaks the
> UART baud rate.

See the top-level `docs/` for the protocol, memory map and full instructions.
