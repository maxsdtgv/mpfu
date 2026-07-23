# MPFU — Microchip PIC Firmware Upgrader

A small custom **bootloader for the PIC16F1789** plus a Linux host uploader.
It lets you reprogram the application over a UART link (and, optionally, from an
external SPI EEPROM) without a hardware programmer after the bootloader itself
has been installed once.

## Highlights

- Bootloader lives at the **end** of flash (`0x3800`–`0x3FDF`); the application
  gets the low region `0x0000`–`0x37FF`.
- **Applications are compiled completely normally** (reset at `0x0000`,
  interrupt vector at `0x0004`). All the relocation work is done by the host at
  flashing time — no `__at()` tricks or custom linker offsets in the app.
- Application interrupts run with **zero added latency** (the ISR stays on the
  hardware interrupt vector `0x0004`).
- Each flashed block is **verified** by read-back.

## Repository layout

```
mcu_part/v1.2.X/   Bootloader firmware (XC8 C / MPLAB X project)
host_part/cpp/     Linux host uploader "mpfu" (C++), + local test harness
test/blink_irq/    Example application (Timer0 ISR blinker) used as a fixture
test/led_blink.X/  Older sample app (kept for reference)
sim/               Proteus simulation project
docs/              Documentation (this folder) + silicon errata PDF
```

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
mcu_part/v1.2.X/flash.sh

# 2. Build the host uploader
make -C host_part/cpp

# 3. Flash an application over UART through the bootloader
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -f test/blink_irq/main.hex -s
```
