# Building and Flashing MPFU

## Prerequisites

- **XC8** compiler (v2.x) for the bootloader firmware — standalone, no IDE
  needed. Default path used by the scripts: `/opt/microchip/xc8/v2.50`.
- **MPLAB X v5.35** — only for its `ipecmd` to drive a **PICkit 3**. Later
  MPLAB X releases dropped PICkit 3 support. Default path:
  `/opt/microchip/mplabx/v5.35`.
- **g++ / make** for the host uploader.
- For local testing without hardware: **socat** and **python3**.

## 1. Build the bootloader firmware

```bash
mcu_part/build.sh
```

This compiles with `-O1` and the memory model
`-mrom=default,-0-37FF,-3FE0-3FFF`, placing the bootloader at `0x3800`–`0x3FDF`.
It prints a memory summary and produces `mpfu.hex` in `/tmp/mpfu_build/`.

Overrides via environment: `XC8=<bin dir>`, `OPT=-O0`, `ROM=...`, `OUT_DIR=...`.

## 2. Flash the bootloader with a PICkit 3

```bash
mcu_part/flash.sh                 # build (build.sh) then flash
mcu_part/flash.sh path/to/fw.hex  # flash an existing hex
VERIFY_ONLY=1  mcu_part/flash.sh  # check programmer/target only
RELEASE_ONLY=1 mcu_part/flash.sh  # release the device from reset (run app)
```

Notes:

- `flash.sh` calls `ipecmd.jar` with MPLAB X's **bundled Java 8**. The installed
  `ipecmd.sh` wrapper is not usable here because its `jdkhome` was never patched,
  so it falls back to the system JVM and fails with "Hex File could not be read".
- Overrides: `MPLABX=<dir>`, `JAVA8=<java bin>`, `TOOL=PPK3`, `DEV=16F1789`.

## 3. Build the host uploader

```bash
make -C host_part/cpp        # produces host_part/cpp/mpfu
```

## 4. Flash an application over UART (through the bootloader)

Applications are compiled **normally** (reset at `0x0000`). Example fixture:

```bash
test/blink_irq/build.sh      # produces test/blink_irq/main.hex
```

Enter the bootloader (hold **RB0** at reset until **RE0** lights), then:

```bash
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -f test/blink_irq/main.hex -s
```

- `-f <hex>` program an application, `-s` start it afterwards, `-r <file>` read
  the whole flash back to a file, `-v` verbose.
- The bootloader preserves its jump at `0x0000`, keeps the app ISR at `0x0004`,
  relocates the app reset vector to `0x3FFC`, protects its own code region, and
  the host verifies every block by read-back.

## 5. Autonomous update from EEPROM (ExtUpgrade)

This path programs flash from an image staged in the external SPI EEPROM, with
no host attached during programming — the basis for over-the-air updates.

```bash
# Build an EEPROM image from a normally-compiled application (offline):
host_part/cpp/mpfu -g test/blink_irq/main.hex /tmp/app.img

# Write the image into the EEPROM (through the bootloader over UART):
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -e /tmp/app.img
```

Then set the bootloader flags and reset the MCU:
- `IsExtUpgrade` (0x3FE1 low byte = 0x00)
- `EXT_ADDR` (0x3FE2/3) = the EEPROM byte address of the image

On the next reset the bootloader reads the image header (`MPFU` magic,
flash address, length), programs the data into flash through the same block
writer used by the UART path, records a status code at `0x3FE6`
(00 = OK, 01 = bad magic, 02 = bad CRC), clears `IsExtUpgrade`, and starts the
application.

Image format: a 64-byte header (magic, version, flash word-address, data
length, Fletcher-16) followed by the dense image data. See
`host_part/cpp/eeimage.h`.

### Optional integrity check (USE_FLETCHER)

A Fletcher-16 verify of the EEPROM image *before* touching flash is available
but **disabled by default** on the PIC16F1789 (it costs ~130 words and needs
the bootloader ROM region extended below `0x3800`, e.g.
`ROM=default,-0-35FF,-3FE0-3FFF ./build.sh` built with `-DUSE_FLETCHER`).
On this part, integrity of an over-the-air image can instead be checked by
reading the EEPROM back over the same link and verifying on the host. Enable
`USE_FLETCHER` (in `mcu_part/bootloader.h`) on parts with more flash.

## Local testing without hardware

```bash
host_part/cpp/mock_mcu.py    # protocol mock + self-test
host_part/cpp/test_e2e.sh    # socat virtual serial pair + mock MCU + real ./mpfu
```

`test_e2e.sh` flashes the `blink_irq` fixture into a mock MCU and checks that the
bootloader jump at `0x0000` survives, the app data lands from `0x0004`, and the
read-back verifies.

## Troubleshooting

- **"Hex File could not be read"** from ipecmd → wrong Java; use the bundled
  Java 8 (flash.sh does this).
- **"Device not found"** over UART → make sure the bootloader is resident
  (RE0 lit; hold RB0 at reset) and the baud rate is **115200**.
- **Chip keeps resetting / RE0 hard to catch** → normal when no application is
  present (STVREN reset loop); flash an app or hold RB0 to stay in the loader.
