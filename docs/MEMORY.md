# MPFU Memory Map & Vector Relocation

Target: **PIC16F1789** — 16K words of flash, word addresses `0x0000`–`0x3FFF`.
On this enhanced mid-range core the reset vector is fixed at `0x0000` and the
interrupt vector at `0x0004` (hardware).

## Flash layout

```
0x0000 - 0x0003   Reset vector -> bootloader's own trampoline (MOVLP;GOTO,
                  possibly two-step through 0x0002). Preserved on every write.
0x0004 - 0x37BF   APPLICATION (compiled normally from 0x0000; ISR sits at 0x0004)
0x37C0 - 0x3FBF   BOOTLOADER code (2048 words)
0x3FC0 - 0x3FDF   Bootloader "flags" row (one 32-word flash row):
                    0x3FC0  IsBLStart      (low byte 0x00 = stay in BL)
                    0x3FC1  IsExtUpgrade   (low byte 0x00 = upgrade from ext EEPROM)
                    0x3FC2  ExtUpgrade EEPROM image addr, high
                    0x3FC3  ExtUpgrade EEPROM image addr, low
                    0x3FC4  ExtUpgrade status code
                    ....    (unused)
0x3FE0 - 0x3FFF   Application reset-vector row. StartApp() jumps to 0x3FE0.
                  Holds a MOVLP;GOTO to the application's real entry point,
                  synthesised by the host; the rest of the row is blank.
```

The bootloader is built into the top of flash with the XC8 memory model:

```
-mrom=default,-0-37BF,-3FC0-3FFF
```

which reserves the low region for the application and the top two rows (flags +
app-vector) for the bootloader. The reset vector at `0x0000` is emitted
automatically by XC8 as a jump to the bootloader entry — no manual vector
injection is needed for the bootloader itself.

### Why the app-vector row is at the very top

If the application vector row is ever blank or bad, the CPU runs off the end of
flash, and `PC` **wraps from `0x3FFF` back to `0x0000`** — which is the
bootloader's own reset jump. So a broken vector self-recovers into the
bootloader instead of falling through into the flags row (whose bytes would be
executed as random instructions). This is why the vector row sits *above* the
flags row, not below it.

## Why the flash is written in 32-word rows

PIC16F1 program memory is erased and written a **row (32 words) at a time**,
aligned to a `0x20` boundary (`EEADRL[4:0] = 0`). Only the low 5 address bits
select which write latch inside the row is loaded; if a block does not start on
a 32-word boundary, addresses wrap **inside the same row** instead of advancing.

Therefore the host always emits **full, 0x20-aligned, 32-word blocks**, padding
unused words with the blank value `0x3FFF`.

## Vector relocation — how a normal app coexists with the bootloader

Goal: the application is compiled **normally** (reset at `0x0000`, ISR at
`0x0004`), yet the bootloader must always get control first at power-up.

All the platform-specific work is done by the **host** when it builds the image
(`fw_converter.cpp`). The bootloader stays a dumb, chip-specific writer.

The host:

1. Parses the application HEX and follows its reset-vector **jump chain** to the
   real entry point. XC8 emits the reset vector differently depending on whether
   the app has an interrupt handler:
   - **with an ISR**, `0x0004` is occupied, so `start` is pushed high and the
     reset vector is a single `MOVLP;GOTO <high addr>`;
   - **without an ISR**, the linker places `start` right at `0x0002`, so the
     reset vector is a two-step trampoline `GOTO 0x002` -> `GOTO <entry>`.
   The host decodes `MOVLP (0x3180|k)` / `GOTO (0x2800|a)`
   (`target = (PCLATH<7:3> << 11) | (a & 0x7FF)`) and follows hops while the
   target lands inside the reserved words `0x0000`–`0x0003`, stopping at the
   real entry.
2. Synthesises a canonical `MOVLP;GOTO <entry>` and places it as the block for
   the **app-vector row** (`0x3FE0`).
3. Emits every populated application row as an ordinary block (row 0 included),
   skipping the bootloader code region and the flags row.

The bootloader, in `WriteAppBlock`, only enforces protection:

- **reset row (`0x0000`)**: keeps the existing words `0x0000`–`0x0003` (its own
  trampoline) and writes the app's data for words `0x0004`–`0x001F`;
- **bootloader code (`0x37C0`–`0x3FBF`)**: refused;
- **flags row (`0x3FC0`)**: refused (only `SET_EXT_UPGRADE` may touch it);
- **anything else** (including the app-vector row `0x3FE0`): written as-is.

The exact same rules apply to both update paths — UART and autonomous EEPROM
(ExtUpgrade) — because both feed blocks through `WriteAppBlock`.

### Boot sequence

```
Power-up / reset
   -> 0x0000: jump to bootloader entry (in 0x37C0-0x3FBF)
   -> bootloader reads its flags:
        - if IsExtUpgrade set  -> program flash from the external EEPROM;
                                  start the app ONLY if the upgrade succeeded,
                                  otherwise stay in the bootloader (recoverable)
        - if RB0 held OR IsBLStart set -> stay in the bootloader (serve UART), light RE0
                                          + arm the ~256 s inactivity watchdog:
                                            no traffic -> reset -> the app boots
        - otherwise             -> StartApp()
   -> StartApp(): disable the watchdog, then GOTO 0x3FE0
   -> 0x3FE0: MOVLP;GOTO <app entry> -> application entry point
```

Because the interrupt vector at `0x0004` holds the application's real ISR, the
application's interrupts work with no extra jump or latency once it is running.

## OTA from the application's runtime (IMPORTANT)

In a fielded product there is **no host**. The running application is responsible
for the whole hand-off to the bootloader. The sequence the application must
perform is:

1. **Receive** the new firmware image (image v2 — see docs/PROTOCOL.md) over its
   own link (e.g. Wi-Fi) and **write it into the external EEPROM** (25LC512),
   starting at some byte address `A` (typically `0x0000`).
2. **Set the bootloader flags** in the flags row so the bootloader knows to run
   the upgrade on the next start. The flags live in flash row `0x3FC0`; only the
   **low byte** of each word is significant. The application must write these
   words using its **own** flash-write routine (a read-modify-write of the
   `0x3FC0` row — the bootloader will not do it for the app, and `WRITE_TO_MEM`
   over UART refuses this row):

   | Word address | Field          | Value to write (low byte)              |
   |--------------|----------------|----------------------------------------|
   | `0x3FC1`     | `IsExtUpgrade` | `0x00`  (0x00 = set / "do the upgrade")|
   | `0x3FC2`     | ExtAddr high   | `(A >> 8) & 0xFF`                       |
   | `0x3FC3`     | ExtAddr low    | `A & 0xFF`                             |

   Leave `0x3FC0` (`IsBLStart`) and the rest of the row unchanged.
3. **Reset** the MCU (e.g. `asm("reset")`, or let the WDT fire).

On restart the bootloader reads the flags, sees `IsExtUpgrade`, reads the image
from EEPROM address `A`, programs flash, records a status code at **`0x3FC4`**
(`00`=OK, `01`=bad magic, `02`=bad CRC, `03`=bad device, `04`=bad version),
clears `IsExtUpgrade`, and — **only if the status is OK** — starts the new
application. On any error it stays in the bootloader so the unit is recoverable.

> The host's `-u [addr]` option (command `SET_EXT_UPGRADE`, 0x16) does exactly
> steps 2–3 from a PC, for development and production programming. The runtime
> application does the same thing itself, without a host.

## Forcing the bootloader from the application (without RB0)

An application can hand control to the bootloader on the next start — so a
firmware update needs no physical access to the **RB0** button. There are two
ways in, and both end up doing the same thing:

- **On request over the app's own link** — the host sends `ENTER_BOOTLOADER`
  (`0x1A`) and the *application* handles it (`mpfu --goto-bl`). This command is
  implemented by the application, not the bootloader; see
  `test/common/app_bootentry.c` for a reference implementation the fixtures use.
- **On the application's own decision** — e.g. an OTA controller decides it is
  time to update.

Either way the application must:

1. Set `IsBLStart`: write **`0x00`** to the low byte of word **`0x3FC0`** — a
   read-modify-write of the `0x3FC0` row using the application's **own** flash
   routine (the bootloader will not do it for a running app, and `WRITE_TO_MEM`
   over UART refuses that row).
2. **Reset** the MCU (`asm("reset")`).

On restart the boot check is `if (!KeyBLRequired() && !IsBLStart) StartApp();`,
so with `IsBLStart` set the bootloader stays in its command loop (RE0 on) and
serves UART. `IsBLStart` is **one-shot**: the bootloader clears it immediately
after entering, so the next reset boots the application normally.

> An application that talks to the bootloader over the same UART must use the
> same clock setup as the bootloader (`OSCCON = 0x7A`, SCS = `1x`). Applications
> flashed through the bootloader **inherit the bootloader's configuration words**,
> and those have `PLLEN = ON`: with SCS = `00` the 4x PLL engages, Fosc changes
> and the baud rate is wrong. See docs/HARDWARE.md.

### Inactivity timeout — the bootloader always returns to the application

The bootloader cannot strand a unit in the field. The watchdog "dead-man" is
armed in **both** places where the bootloader can be left waiting or working
without a host:

- **The UART command loop** — timeout **~256 s (4.3 min)**, petted only when a
  frame actually arrives. After that much silence the MCU resets;
  `IsBLStart` has already been cleared, so the application boots.
- **The autonomous EEPROM upgrade (ExtUpgrade)** — armed before the upgrade
  starts and petted after each block, so steady progress is fine but a genuinely
  stuck transfer (dead SPI bus, EEPROM not answering) resets the unit instead of
  hanging forever. This matters because an OTA update runs with no host attached.
  The `IsExtUpgrade` request is cleared **before** the attempt, so a hang cannot
  turn into an endless reset-and-retry loop.

If the image is rejected (bad magic/CRC/device/version) the flash is left
untouched and the bootloader deliberately **stays in its command loop** so a host
can intervene; the status code remains in the flags row. If no host appears, the
inactivity timeout resets the unit and the **previous, still intact application**
boots. A failed OTA therefore never runs garbage and never bricks the device.

This is safe for applications because the part is built with **`WDTE = SWDTEN`**:
the watchdog is off unless software enables it. The bootloader enables it when it
enters the command loop and `StartApp()` disables it again, so **applications
never have to service a watchdog they did not ask for**.

256 s is the hardware maximum: the WDT is clocked from the 31 kHz LFINTOSC and
`WDTCON.WDTPS<4:0>` is its whole divider, with `10010` = 1:8388608 the longest
setting (higher values are *reserved* and fall back to the 1 ms minimum). To
exercise the mechanism quickly, build a shorter timeout:

```bash
EXTRA="-DWDT_BL_TIMEOUT_CONF=0x1B" mcu_part/build.sh    # ~8 s instead of ~256 s
```


## Notes

- The flags row is a normal 32-word row. The bootloader updates it with a
  read-modify-write of the whole row (it self-clears `IsBLStart` after entering,
  so the "stay in bootloader" request is one-shot). Applications never write it
  via `WRITE_TO_MEM`; the only sanctioned modification is the `SET_EXT_UPGRADE`
  command (or the application writing it directly through its own flash routines
  in a production OTA design).
- `WRT` self-write protection on this 16K part only covers up to `0x1FFF`, so the
  bootloader region near `0x3FFF` cannot be hardware write-protected. Protection
  is instead enforced in software by `WriteAppBlock`.
- If the optional `USE_FLETCHER` integrity check is enabled, the bootloader grows;
  if it no longer fits `0x37C0`–`0x3FBF`, move `bl_code_start` down further in the
  device profile and adjust `-mrom` accordingly. See
  docs/BUILD_AND_FLASH.md.
