# MPFU — Microchip PIC Firmware Upgrader

A small custom **bootloader for the PIC16F1789** plus a Linux host uploader.
It lets you reprogram the application over a UART link (and, optionally, from an
external SPI EEPROM) without a hardware programmer after the bootloader itself
has been installed once.

## Highlights

- Bootloader lives at the **end** of flash (`0x3800`–`0x3FDF` by default); the
  application gets the low region `0x0000`–`0x37FF`.
- **Applications are compiled completely normally** (reset at `0x0000`,
  interrupt vector at `0x0004`). All the relocation work is done by the
  bootloader itself — no `__at()` tricks or custom linker offsets in the app.
- Application interrupts run with **zero added latency** (the ISR stays on the
  hardware interrupt vector `0x0004`).
- Each flashed block is **verified** by read-back.
- **Two update paths:**
  - **UART** — host `mpfu` streams an application over the serial link.
  - **Autonomous (ExtUpgrade)** — a firmware image staged in an external SPI
    EEPROM (25LC512) is programmed into flash on the next reset when the
    `IsExtUpgrade` flag is set. This is the basis for over-the-air updates
    (e.g. a Wi-Fi module writes the image to EEPROM, sets the flag, resets).
- **Self-protection:** the bootloader keeps its own jump at `0x0000-0x0003`,
  refuses writes into its code region, and relocates the app reset vector to
  `0x3FFC` — the same rules apply to both update paths.

## Host tool (`mpfu`) options

```
-D <port>    serial port (e.g. /dev/ttyUSB0)
-b <baud>    serial speed (115200)
-f <hex>     flash an application over UART
-s           start the application after flashing
-r <file>    read the whole flash back to a file
-e <file>    write a raw binary (or EEPROM image) into the external EEPROM
-g <in.hex> <out.img>   generate an EEPROM firmware image (offline, no device)
-v           verbose
```

## Repository layout

```
mcu_part/          Bootloader firmware (XC8 C)
host_part/cpp/     Linux host uploader "mpfu" (C++) + eeimage format + tests
test/blink_irq/    Example application (Timer0 ISR blinker) used as a fixture
test/led_blink.X/  Older sample app (kept for reference)
sim/               Proteus simulation project
docs/              Documentation
releases/          Pre-built, versioned release artifacts
```

> The PIC16(L)F1788/9 silicon errata (Microchip DS80000575) is a useful
> reference but is not bundled here — download it from microchip.com if needed.

## Documentation

- [docs/PROTOCOL.md](docs/PROTOCOL.md) — UART frame protocol and commands.
- [docs/MEMORY.md](docs/MEMORY.md) — flash memory map and the vector-relocation
  scheme that makes normal apps "just work".
- [docs/HARDWARE.md](docs/HARDWARE.md) — MCU configuration, pin assignment,
  real clock/UART/SPI settings.
- [docs/BUILD_AND_FLASH.md](docs/BUILD_AND_FLASH.md) — how to build both parts
  and flash the bootloader / an application.

## Quick start

```bash
# 1. Build and flash the bootloader with a PICkit 3 (see docs/BUILD_AND_FLASH.md)
mcu_part/flash.sh

# 2. Build the host uploader
make -C host_part/cpp

# 3. Flash an application over UART through the bootloader
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -f test/blink_irq/main.hex -s
```
