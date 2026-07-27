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
`-mrom=default,-0-37FF,-3FC0-3FFF`, placing the bootloader at `0x3800`–`0x3FBF`
and reserving the flags row (`0x3FC0`) and app-vector row (`0x3FE0`).
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

Applications are compiled **normally** (reset at `0x0000`). Example fixtures:

```bash
test/blink_irq/build.sh      # with a Timer0 ISR   -> test/blink_irq/main.hex
test/blink_noirq/build.sh    # no interrupts       -> test/blink_noirq/main.hex
```

Enter the bootloader (hold **RB0** at reset until **RE0** lights), then:

```bash
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -c 16f1789 -f test/blink_irq/main.hex -s
```

### Without the RB0 button (`--goto-bl`)

If the running application implements the `ENTER_BOOTLOADER` command (both
fixtures do — see `test/common/app_bootentry.c`), the host can ask it to hand
over, so no button press is needed:

```bash
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -c 16f1789 --goto-bl -f app.hex -s
```

The application ACKs, sets `IsBLStart` and resets; the bootloader comes up and the
update proceeds. It also combines with the EEPROM path:

```bash
host_part/cpp/mpfu -D ... -c 16f1789 --goto-bl -e app.img -u 0
```

If the device happens to be in the bootloader already, `--goto-bl` is harmless
(the bootloader answers "unknown command" and the host carries on).

> The bootloader never keeps a unit hostage: after **~4.3 min** without any host
> traffic its watchdog resets the MCU and the application starts again. See
> docs/MEMORY.md. Applications need no watchdog handling of their own
> (`WDTE = SWDTEN`), but they must use the same clock setup as the bootloader
> (`OSCCON = 0x7A`) — see docs/HARDWARE.md.

- `-c <profile>` device profile (default `16f1789`; see `configs/<name>.conf`).
- `-f <hex>` program an application, `-s` start it afterwards, `-r <file>` read
  the whole flash back to a file, `-v` verbose.
- The host reads the chip's Device ID and refuses to flash if it does not match
  the profile (unless the profile's `device_id` is the `0xFFFF` wildcard).
- The host resolves the application's real entry point, builds an image (see
  §6), and streams the blocks. The bootloader preserves its own trampoline at
  `0x0000`, keeps the app ISR at `0x0004`, protects its code region and flags
  row, and the host verifies every block by read-back.

## 5. Autonomous update from EEPROM (ExtUpgrade)

This path programs flash from an image staged in the external SPI EEPROM, with
no host attached during programming — the basis for over-the-air updates.

```bash
# Build a firmware image from a normally-compiled application (offline):
host_part/cpp/mpfu -c 16f1789 -g test/blink_irq/main.hex /tmp/app.img

# Write the image into the EEPROM, then ARM the upgrade and reset — one command:
host_part/cpp/mpfu -D /dev/ttyUSB0 -b 115200 -c 16f1789 -e /tmp/app.img -u 0
```

- `-e <img>` writes the image into the external EEPROM (verified block by block).
- `-u [addr]` sends `SET_EXT_UPGRADE`: the bootloader sets `IsExtUpgrade` +
  the image address in the flags row and **resets** (addr defaults to 0).
- `-E <file> [addr] [nbytes]` reads the external EEPROM back to a file (for
  debugging). With no `nbytes`, the length is auto-detected from the image
  header at `addr` (exactly `64 + block_count*66` bytes); give `nbytes` to dump
  a raw range. The written file is a byte-for-byte copy and can be fed back to
  `-e`.

On the next reset the bootloader reads the image header (`MPFU` magic, device_id,
block_count, Fletcher-16), programs each block to its address through the same
`WriteAppBlock` used by the UART path, records a status code in the flags row
(`0x3FC4`: 00 = OK, 01 = bad magic, 02 = bad CRC, 03 = bad device, 04 = bad
version), clears `IsExtUpgrade`, and starts the application **only if the upgrade
succeeded**.

If the image is rejected the flash is left untouched and the bootloader stays in
its command loop so a host can intervene; if none does, its watchdog resets the
unit after ~4.3 min and the **previous application** (still intact) boots. The
watchdog also covers the upgrade itself, so a stuck SPI transfer cannot hang a
unit that is updating with no host attached.

In a production OTA design the application itself stages the image in EEPROM,
sets the flag, and resets — no host needed. `-u` is the host/debug equivalent.

## 6. Firmware image format (image v2)

Both `-f` (UART) and `-g`/`-e` (EEPROM) use the same image: a 64-byte header
(`MPFU` magic, format version, device_id, block_count, Fletcher-16) followed by
`block_count` entries of `{addr(2), 64 data bytes}`. The host resolves the app
reset vector and emits all blocks including the app-vector row at `0x3FE0`; the
bootloader just writes each block. See `host_part/cpp/imagev2.h` and
docs/MEMORY.md.

### Optional bootloader checks (feature flags)

Three checks in `mcu_part/bootloader.h` are **off by default** to save flash;
enable per build with `OPT="-O1 -D<FLAG>"`:

- `USE_DEVICE_ID_CHECK` — bootloader verifies the image `device_id` against its
  own DEVID (`0x8006`) before an EEPROM upgrade.
- `USE_VERSION_CHECK` — verifies the image `format_version`.
- `USE_FLETCHER` — verifies the image Fletcher-16 before touching flash.

Measured sizes on 16F1789 (region `0x3800`–`0x3FBF` = 1984 words): base ≈ 1950
words; with the id/version checks ≈ 1960. If a combination no longer fits, move
`bl_code_start` down one row in the device profile and adjust `-mrom`.

## Local testing without hardware

```bash
host_part/cpp/mock_mcu.py    # protocol mock + self-test
host_part/cpp/test_e2e.sh    # socat virtual serial pair + mock MCU + real ./mpfu
```

`test_e2e.sh` flashes the `blink_noirq` fixture into a mock MCU and checks that
the bootloader trampoline at `0x0000` survives, the app-vector row `0x3FE0` is
written, and the read-back verifies.

## Troubleshooting

- **"Hex File could not be read"** from ipecmd → wrong Java; use the bundled
  Java 8 (flash.sh does this).
- **"Device not found"** over UART → make sure the bootloader is resident
  (RE0 lit; hold RB0 at reset) and the baud rate is **115200**.
- **Chip keeps resetting / RE0 hard to catch** → normal when no application is
  present (STVREN reset loop); flash an app or hold RB0 to stay in the loader.
