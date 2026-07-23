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
mcu_part/v1.2.X/build.sh
```

This compiles with `-O1` and the memory model
`-mrom=default,-0-37FF,-3FE0-3FFF`, placing the bootloader at `0x3800`–`0x3FDF`.
It prints a memory summary and produces `mpfu.hex` in `/tmp/mpfu_build/`.

Overrides via environment: `XC8=<bin dir>`, `OPT=-O0`, `ROM=...`, `OUT_DIR=...`.

## 2. Flash the bootloader with a PICkit 3

```bash
mcu_part/v1.2.X/flash.sh                 # build (build.sh) then flash
mcu_part/v1.2.X/flash.sh path/to/fw.hex  # flash an existing hex
VERIFY_ONLY=1  mcu_part/v1.2.X/flash.sh  # check programmer/target only
RELEASE_ONLY=1 mcu_part/v1.2.X/flash.sh  # release the device from reset (run app)
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
- The host preserves the bootloader jump at `0x0000`, keeps the app ISR at
  `0x0004`, relocates the app reset vector to `0x3FFC`, and verifies every block
  by read-back.

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
