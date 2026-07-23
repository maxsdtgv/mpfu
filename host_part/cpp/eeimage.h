#ifndef MPFU_EEIMAGE_H
#define MPFU_EEIMAGE_H
/*
 * MPFU EEPROM firmware-image format (for the bootloader's ExtUpgrade path).
 *
 * An "EEPROM image" is what the host stages into the external 25LC512 so the
 * bootloader can program it into flash on the next reset (flag IsExtUpgrade).
 * It is NOT Intel HEX — it is a compact, contiguous layout:
 *
 *   [ HEADER : 64 bytes ]
 *     off 0  magic[4]   = 'M','P','F','U'
 *     off 4  version    = 0x01
 *     off 5  flash_addr (2 bytes, big-endian) : destination WORD address in flash
 *     off 7  data_len   (2 bytes, big-endian) : number of DATA bytes (multiple of 64)
 *     off 9  fletcher16 (2 bytes, big-endian) : checksum over the DATA bytes
 *     off 11..63         reserved (0xFF)
 *   [ DATA : data_len bytes ]  contiguous flash image, written from flash_addr
 *
 * DATA is a dense image (gaps padded with 0x3FFF words, like the flash
 * converter output), written to flash in 64-byte (32-word) blocks starting at
 * flash_addr. The bootloader verifies magic + Fletcher-16 BEFORE touching flash.
 */

#define EEIMG_HEADER_SIZE     64
#define EEIMG_MAGIC0          'M'
#define EEIMG_MAGIC1          'P'
#define EEIMG_MAGIC2          'F'
#define EEIMG_MAGIC3          'U'
#define EEIMG_VERSION         0x01

#define EEIMG_OFF_MAGIC       0
#define EEIMG_OFF_VERSION     4
#define EEIMG_OFF_FLASH_ADDR  5
#define EEIMG_OFF_DATA_LEN    7
#define EEIMG_OFF_CRC         9

#define EEIMG_BLOCK_BYTES     64   /* 32 words per flash row */

/* Fletcher-16 over a byte buffer. Simple, table-free, far stronger than parity. */
static inline unsigned short fletcher16(const unsigned char *data, unsigned int len)
{
    unsigned short s1 = 0, s2 = 0;
    unsigned int i;
    for (i = 0; i < len; i++) {
        s1 = (unsigned short)((s1 + data[i]) % 255);
        s2 = (unsigned short)((s2 + s1) % 255);
    }
    return (unsigned short)((s2 << 8) | s1);
}

#endif /* MPFU_EEIMAGE_H */
