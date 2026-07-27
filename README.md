# MPFU — Microchip PIC Firmware Upgrader

A small custom **bootloader for the PIC16F1789** plus a Linux host uploader.
It lets you reprogram the application over a UART link (and, optionally, from an
external SPI EEPROM) without a hardware programmer after the bootloader itself
has been installed once.

## Highlights

- Bootloader lives at the **end** of flash (`0x3800`–`0x3FBF` by default); the
  application gets the low region `0x0000`–`0x37FF`.
- **Applications are compiled completely normally** (reset at `0x0000`,
  interrupt vector at `0x0004`). No `__at()` tricks or custom linker offsets.
- The **host** resolves the application's real entry point (following the XC8
  reset-vector jump chain) and builds a firmware image; the **bootloader is a
  small, chip-specific writer** that just programs blocks and protects itself.
  Platform knowledge lives in host-side **device profiles** (`-c 16f1789`).
- Application interrupts run with **zero added latency** (the ISR stays on the
  hardware interrupt vector `0x0004`).
- Each flashed block is **verified** by read-back.
- **Two update paths, one image format:**
  - **UART** — host `mpfu` streams an application over the serial link.
  - **Autonomous (ExtUpgrade)** — a firmware image staged in an external SPI
    EEPROM (25LC512) is programmed into flash on the next reset. This is the
    basis for over-the-air updates (e.g. a Wi-Fi module writes the image to
    EEPROM, sets the flag, resets). A bad image never runs: the bootloader only
    launches the app if the upgrade verified.
- **Self-protection:** the bootloader keeps its own reset trampoline at
  `0x0000-0x0003`, refuses writes into its code region and its flags row, and
  the same rules apply to both update paths.
- **No button required:** a running application can be asked over UART to hand
  over to the bootloader (`mpfu --goto-bl`), so updates need no physical access
  to the RB0 pin. And the bootloader can never strand a unit — after ~4.3 min
  with no host traffic its watchdog resets the MCU back into the application.

## Host tool (`mpfu`) options

```
-D <port>    serial port (e.g. /dev/ttyUSB0)
-b <baud>    serial speed (115200)
-c <name>    device profile (default 16f1789; configs/<name>.conf)
-f <hex>     flash an application over UART
-s           start the application after flashing
-r <file>    read the whole flash back to a file
-e <file>    write a firmware image into the external EEPROM
-g <in.hex> <out.img>   generate a firmware image (offline, no device)
-E <file> [addr] [nbytes]   read the external EEPROM to a file
             (addr default 0; nbytes default = auto from the image header)
-u [addr]    arm the autonomous EEPROM upgrade at EEPROM addr (default 0) and reset
--goto-bl    ask the RUNNING application to enter the bootloader (no RB0 needed)
-v           verbose
```

## Repository layout

```
mcu_part/           Bootloader firmware (XC8 C)
host_part/cpp/      Linux host uploader "mpfu" (C++): image v2, device profiles, tests
host_part/cpp/configs/   Per-device profiles (e.g. 16f1789.conf)
test/blink_irq/     Example app WITH a Timer0 ISR (fixture)
test/blink_noirq/   Example app with NO interrupts (fixture)
sim/                Proteus simulation project
docs/               Documentation
releases/           Pre-built, versioned release artifacts
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
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -c 16f1789 -f test/blink_irq/main.hex -s
```
