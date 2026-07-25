#ifndef MPFU_FW_CONVERTER_H
#define MPFU_FW_CONVERTER_H

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include "uart_procedures.h"
#include "deviceconfig.h"

// One aligned flash block: a row-aligned word address + 32 words (64 bytes,
// big-endian H,L per word). This is the unit both the UART and EEPROM paths use.
struct ImageBlock {
    unsigned int  addr;        // flash WORD address, multiple of cfg.row_words
    unsigned char data[64];    // 32 words, big-endian
};

// Parse an Intel HEX application and build the sparse list of flash blocks for
// the given device, resolving the reset-vector relocation:
//   * every populated row in [firstRow..lastRow] is emitted (gaps padded blank),
//     skipping the bootloader code region and the flags row;
//   * an extra block at cfg.app_vector_row is synthesised holding a canonical
//     MOVLP;GOTO to the application's real entry point (found by walking the
//     reset-vector jump chain past the reserved trampoline words).
// Returns 0 on success; non-zero on error (err filled with a message).
int buildImageV2Blocks(const char *inHexPath, const DeviceConfig *cfg,
                       std::vector<ImageBlock> &blocks, std::string &err);

// Serialise blocks into an image-v2 byte buffer (64-byte header + N*66 blocks).
int serializeImageV2(const std::vector<ImageBlock> &blocks, unsigned int device_id,
                     std::vector<unsigned char> &out);

// Convenience: build blocks and write an image-v2 file (used by -g and -e).
int writeImageV2File(const char *inHexPath, const char *outImgPath,
                     const DeviceConfig *cfg);

#endif // MPFU_FW_CONVERTER_H
