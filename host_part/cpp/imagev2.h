#ifndef MPFU_IMAGEV2_H
#define MPFU_IMAGEV2_H
/*
 * MPFU unified firmware image, format v2.
 *
 * ONE format is used for both delivery paths:
 *   - streamed to the bootloader over UART (host -f), and
 *   - written verbatim into the external EEPROM (host -e) for the autonomous
 *     ExtUpgrade path.
 *
 * The image is a sparse list of aligned flash blocks. All the platform-specific
 * intelligence (reset-vector relocation, which words to program) is resolved by
 * the HOST when it builds the image; the bootloader just writes each block to
 * its address, enforcing only its own protection rules.
 *
 *   [ HEADER : 64 bytes ]
 *     off 0  magic[4]        = 'M','P','F','U'
 *     off 4  format_version  = 0x02
 *     off 5  device_id      (2, big-endian)  DEVID; 0xFFFF = "any device"
 *     off 7  block_count    (2, big-endian)  number of BLOCK entries that follow
 *     off 9  fletcher16     (2, big-endian)  checksum over the whole BLOCKS area
 *     off 11..63            reserved (0xFF)
 *   [ BLOCKS : block_count entries, 66 bytes each ]
 *     off 0  addr (2, big-endian)  flash WORD address, multiple of row size
 *     off 2  data (64 bytes)       32 words, big-endian (H,L per word)
 *
 * block_count (not a byte length or sentinel) tells the bootloader exactly how
 * many blocks to read — important for the EEPROM path, which has no EOF.
 */

#define IMGV2_MAGIC0            'M'
#define IMGV2_MAGIC1            'P'
#define IMGV2_MAGIC2            'F'
#define IMGV2_MAGIC3            'U'
#define IMGV2_FORMAT_VERSION    0x02
#define IMGV2_DEVICE_ANY        0xFFFF

#define IMGV2_HEADER_SIZE       64
#define IMGV2_OFF_MAGIC         0
#define IMGV2_OFF_VERSION       4
#define IMGV2_OFF_DEVICE_ID     5
#define IMGV2_OFF_BLOCK_COUNT   7
#define IMGV2_OFF_CRC           9

#define IMGV2_BLOCK_DATA_BYTES  64   /* 32 words per flash row */
#define IMGV2_BLOCK_ADDR_BYTES  2
#define IMGV2_BLOCK_SIZE        (IMGV2_BLOCK_ADDR_BYTES + IMGV2_BLOCK_DATA_BYTES) /* 66 */

/* Fletcher-16 over a byte buffer. Simple, table-free, far stronger than parity.
 * The bootloader has a division-free equivalent; keep the two in sync. */
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

#endif /* MPFU_IMAGEV2_H */
