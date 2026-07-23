# MPFU Memory Map & Vector Relocation

Target: **PIC16F1789** — 16K words of flash, word addresses `0x0000`–`0x3FFF`.
On this enhanced mid-range core the reset vector is fixed at `0x0000` and the
interrupt vector at `0x0004` (hardware).

## Flash layout

```
0x0000 - 0x0003   Reset vector -> GOTO bootloader entry
0x0004 - 0x37FF   APPLICATION (compiled normally from 0x0000; ISR sits at 0x0004)
0x3800 - 0x3FDF   BOOTLOADER code (~1.5K words)
0x3FE0 - 0x3FFF   Bootloader "flags" row (one 32-word flash row):
                    0x3FE0  IsBLStart          (low byte 0x00 = stay in BL)
                    0x3FE1  IsExtUpgrade        (low byte 0x00 = upgrade from ext EEPROM)
                    0x3FE2  ExtUpgrade start addr, high
                    0x3FE3  ExtUpgrade start addr, low
                    0x3FE4  ExtUpgrade block count, high
                    0x3FE5  ExtUpgrade block count, low
                    0x3FE6  ExtUpgrade status code
                    ....    (unused)
                    0x3FFC  App reset vector (relocated here by the bootloader)
                    0x3FFD    "
                    0x3FFE    "
                    0x3FFF    "
```

The bootloader is built into the top of flash with the XC8 memory model:

```
-mrom=default,-0-37FF,-3FE0-3FFF
```

which reserves the low region for the application and the flags row for the
bootloader. The reset vector at `0x0000` is emitted automatically by XC8 as a
`GOTO` to the bootloader entry — no manual vector injection is needed for the
bootloader itself.

## Why the flash is written in 32-word rows

PIC16F1 program memory is erased and written a **row (32 words) at a time**,
aligned to a `0x20` boundary (`EEADRL[4:0] = 0`). Only the low 5 address bits
select which write latch inside the row is loaded; if a block does not start on
a 32-word boundary, addresses wrap **inside the same row** instead of advancing.

Therefore the host always sends **full, 0x20-aligned, 32-word blocks**, padding
unused words with the blank value `0x3FFF`. The converter (`fw_converter.cpp`)
turns an arbitrary Intel HEX file into exactly these blocks.

## Vector relocation — how a normal app coexists with the bootloader

Goal: the application is compiled **normally** (reset at `0x0000`, ISR at
`0x0004`), yet the bootloader must always get control first at power-up. This is
handled **by the bootloader itself** (function `WriteAppBlock`), so it applies
identically to both update paths — UART and autonomous EEPROM (ExtUpgrade) — and
the application needs no special build settings.

When a block targeting the reset row (`0x0000`) is written, the bootloader:

1. Keeps the **current** words `0x0000`–`0x0003` (the `GOTO bootloader` jump that
   was installed when the bootloader was programmed). It does not hardcode its
   own address — it just preserves whatever jump is already there.
2. Writes row 0 as `[GOTO bootloader]` (words 0–3) followed by the application's
   data for words `0x0004`–`0x001F`. The app's interrupt handler at `0x0004` is
   kept intact.
3. Relocates the application's **own** reset vector (the incoming words
   `0x0000`–`0x0003`) into the flags row at `0x3FFC`.

For any other block, the bootloader writes it as-is, except it **refuses** any
write into its own code region `0x3800`–`0x3FDF` (self-protection).

The host therefore just sends every 32-word block verbatim; all the special
handling lives in one place in the bootloader.

### Boot sequence

```
Power-up / reset
   -> 0x0000: GOTO bootloader entry (in 0x3800-0x3FDF)
   -> bootloader reads its flags:
        - if IsExtUpgrade set  -> program flash from the external EEPROM, then start app
        - if RB0 held OR IsBLStart set -> stay in the bootloader (serve UART), light RE0
        - otherwise             -> StartApp()
   -> StartApp(): GOTO 0x3FFC
   -> 0x3FFC: the application's original reset vector -> application entry point
```

Because the interrupt vector at `0x0004` holds the application's real ISR, the
application's interrupts work with no extra jump or latency once it is running.

## Notes

- The flags row is a normal 32-word row. The bootloader updates it with a
  read‑modify‑write of the whole row (it self-clears `IsBLStart` after entering,
  so the "stay in bootloader" request is one-shot).
- `WRT` self-write protection on this 16K part only covers up to `0x1FFF`, so the
  bootloader region near `0x3FFF` cannot be hardware write-protected. Protection
  is instead enforced in software by `WriteAppBlock` (it rejects writes into
  `0x3800`–`0x3FDF`).
- If the optional `USE_FLETCHER` integrity check is enabled, the bootloader grows
  past `0x3800` and the ROM region must be extended (e.g. `-0-35FF`); see
  docs/BUILD_AND_FLASH.md.
