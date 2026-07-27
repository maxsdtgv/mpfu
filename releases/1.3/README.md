# MPFU Release 1.3

Pre-built artifacts for **PIC16F1789**.

| File            | Description                                            |
|-----------------|--------------------------------------------------------|
| `mcu_1.3.hex`   | Bootloader firmware (flash with a PICkit 3 via ipecmd) |
| `mpfu_1.3`      | Linux host uploader (x86-64)                           |
| `configs/`      | Device profiles (keep next to the binary)              |

> Target device: **PIC16F1789 only.**

## What's new in 1.3 (vs 1.2)

- **No RB0 button needed.** `mpfu --goto-bl` asks the *running application* to
  hand over to the bootloader: the app ACKs, sets `IsBLStart` and resets. Works
  together with everything else, e.g. `--goto-bl -f app.hex -s` or
  `--goto-bl -e app.img -u 0`.
- **The bootloader always returns to the application.** While waiting for a host
  it runs a watchdog "dead-man" of **~4.3 min** (256 s, the hardware maximum),
  petted only when a frame arrives. After that much silence the MCU resets and the
  application boots — a field unit can no longer be stranded in the bootloader.
- **The watchdog also covers the autonomous EEPROM upgrade**, which is the case
  that matters most: an OTA update runs with no host attached, so a stuck SPI
  transfer or unresponsive EEPROM now resets the unit instead of hanging it. The
  upgrade request is cleared *before* the attempt, so a hang cannot become an
  endless reset-and-retry loop.
- **A failed OTA never runs garbage and never bricks the unit.** A rejected image
  leaves flash untouched; the bootloader waits for a host and, if none comes, the
  watchdog returns the device to the previous (intact) application.
- Built with **`WDTE = SWDTEN`**, so the watchdog is off unless the bootloader
  enables it: **applications need no watchdog handling of their own.**
- New **`ENTER_BOOTLOADER` (0x1A)** command, implemented by the *application*.
  `test/common/app_bootentry.c` in the source tree is a reference implementation
  (UART init, frame poll, flags-row read-modify-write, reset) that real firmware
  can copy.
- Test fixtures (`blink_irq`, `blink_noirq`) now support `--goto-bl`.
- `mcu_part/build.sh` gained `EXTRA=` for extra compiler flags, e.g. a short
  watchdog for testing: `EXTRA="-DWDT_BL_TIMEOUT_CONF=0x1B" ./build.sh` (~8 s).

Compatible with 1.2 images (format v2 unchanged). The bootloader itself must be
reflashed with a programmer to get the new behaviour, because its configuration
word changes (`WDTE`).

> **If you write your own application:** use `OSCCON = 0x7A` (SCS = `1x`), the
> same as the bootloader. Applications inherit the bootloader's configuration
> words, which have `PLLEN = ON`; with `SCS = 00` the 4x PLL engages, Fosc changes
> and UART baud rates break.

## Verified on real hardware

- UART update of apps **with and without** an ISR, with and without RB0.
- `--goto-bl` handover from a running application (ACK first attempt).
- Watchdog dead-man: bootloader left idle resets itself back into the app.
- Autonomous EEPROM upgrade (`-e` + `-u`) end to end, no button.

Bootloader size: 1976 of 16384 words (~12%).

## Quick start

```bash
# 1. Flash the bootloader once with a PICkit 3
J8=/opt/microchip/mplabx/v5.35/sys/java/jre1.8.0_181/bin/java
IPE=/opt/microchip/mplabx/v5.35/mplab_platform/mplab_ipe
"$J8" -jar "$IPE/ipecmd.jar" -TPPK3 -P16F1789 -M -Fmcu_1.3.hex -OL

# 2. First application: hold RB0 and reset, then
./mpfu_1.3 -D /dev/ttyUSB0 -b 115200 -c 16f1789 -f your_app.hex -s

# 3. From then on, no button (if the app implements ENTER_BOOTLOADER)
./mpfu_1.3 -D /dev/ttyUSB0 -b 115200 -c 16f1789 --goto-bl -f your_app.hex -s
```

See the top-level `docs/` for the protocol, memory map and full instructions.
