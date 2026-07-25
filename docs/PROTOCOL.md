# MPFU UART Protocol

The host and bootloader exchange fixed-format frames over the MCU's EUSART.

## Frame format

```
Host  -> MCU :  0x55  LEN  CMD  DATA...
MCU   -> Host:  0xAA  LEN  CMD  DATA...
```

| Field  | Size    | Meaning                                                      |
|--------|---------|-------------------------------------------------------------|
| PREAM  | 1 byte  | `0x55` host→MCU, `0xAA` MCU→host                            |
| LEN    | 1 byte  | Frame length counted **from the LEN byte** (so 4 + data)    |
| CMD    | 1 byte  | Command / response code (see below)                         |
| DATA   | ≤ 64 B  | Payload (32 words, big-endian, for memory operations)       |

- A full data block is **32 words = 64 bytes**, so a full write frame has
  `LEN = 0x44` (4 header bytes + 64 data bytes).
- Word data is big-endian: high byte then low byte of each 14-bit word.
- Serial settings: **115200 baud, 8N1** (see docs/HARDWARE.md).

## Commands (host → MCU)

| Code   | Name                     | Payload                                   |
|--------|--------------------------|-------------------------------------------|
| `0x02` | READ_FROM_MEM            | addrH addrL — read 32 words (or 1 word if addrH=0x80, config space) |
| `0x04` | WRITE_TO_MEM             | addrH addrL + 64 data bytes — program one 32-word row |
| `0x12` | READ_FROM_SERIAL_EEPROM  | addrH addrL — read a block from the external 25LC512 |
| `0x14` | WRITE_TO_SERIAL_EEPROM   | addrH addrL + data — write to the external 25LC512 |
| `0x16` | SET_EXT_UPGRADE          | addrH addrL — arm autonomous EEPROM upgrade at that EEPROM image address, then **reset** |
| `0x0F` | START_APPLICATION        | none — leave the bootloader and run the app |

## Response codes (MCU → host)

| Code   | Meaning                                             |
|--------|-----------------------------------------------------|
| `0xEE` | Success (no data)                                   |
| `0xFF` | Error / unknown command                             |
| other  | Echo of the request command code followed by data (e.g. read results) |

## Addressing notes

- Program memory addresses are **word addresses** (`0x0000`–`0x3FFF`).
- `addrH = 0x80` selects the configuration space; a READ then returns a single
  word (e.g. Device ID at `0x8006`).
- Writes MUST target a **32-word aligned** address (`addr & 0x1F == 0`). The
  PIC16F1 flash erases/writes a whole 32-word row; the host converter guarantees
  aligned, full blocks (see docs/MEMORY.md).

## Examples

```
# Read Device ID (config word 0x8006)
TX: 55 04 02 80 06
RX: AA 04 02 30 2A            # value 0x302A

# Read 32 words from 0x0020
TX: 55 04 02 00 20
RX: AA 42 02 <64 data bytes>

# Program one row at 0x0020 (64 data bytes)
TX: 55 44 04 00 20 <64 data bytes>
RX: AA 02 EE                  # success

# Start the application
TX: 55 02 0F
(no response; control jumps to the application)

# Arm autonomous EEPROM upgrade from image at EEPROM address 0x0000
TX: 55 04 16 00 00
RX: AA 02 EE                  # ACK, then the MCU resets and, on restart,
                              # programs flash from the EEPROM image and
                              # launches the app (if the image verified)
```

## Firmware image format (image v2)

Both the UART flash stream and the external-EEPROM upgrade use the **same**
image: a 64-byte header (`MPFU` magic, format version, device_id, block_count,
Fletcher-16) followed by `block_count` entries of `{addr(2), 64 data bytes}`.
The host resolves the application reset vector and lays out all blocks (including
the app-vector row at `0x3FE0`); the bootloader just programs each block to its
address. See `host_part/cpp/imagev2.h` and docs/MEMORY.md.

The `SET_EXT_UPGRADE` command is the sanctioned way to arm the autonomous
(ExtUpgrade) path from the host — it is the ONLY way to modify the flags row, as
`WRITE_TO_MEM` refuses writes there. In a production OTA design the application
itself sets the flag (after staging an image in EEPROM) and resets.

## Reliability note

The frame currently has **no CRC**. A UART glitch during a write could program
corrupt data. The host mitigates this by reading every block back and comparing
(verify). Adding a CRC16 + NACK/retry is planned, and is important before
delivering firmware over less reliable links (e.g. Wi‑Fi into the external
EEPROM path).
