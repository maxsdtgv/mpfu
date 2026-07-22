#include "fw_converter.h"
#include <map>
#include <limits.h>
using namespace std;

// ---------------------------------------------------------------------------
// Intel HEX -> uniform aligned blocks for the MPFU bootloader.
//
// The bootloader/MCU can only erase+write whole flash ROWS of 32 words aligned
// to a 0x20 word boundary. To make the host logic trivial and safe, this
// converter emits ONLY full, 32-word, 0x20-aligned blocks. Any row that
// contains at least one programmed word is emitted in full; gaps are padded
// with the blank value 0x3FFF.
//
// Output line format (ASCII hex, consumed by main.cpp), unchanged:
//     LL CC AAAA DDDD...
//   LL   = frame length byte = 4 + 64 = 0x44
//   CC   = WRITE_TO_MEM
//   AAAA = word address (row base, multiple of 0x20)
//   DD.. = 64 data bytes = 32 words, big-endian (H,L per word)
// ---------------------------------------------------------------------------

#define ROW_WORDS   32
#define BLANK_WORD  0x3FFF

// Parse one Intel HEX record line. Returns false if not a data/EOF/ext record
// we handle. Updates the word map `mem` (word address -> 14-bit word).
static void parseIntelHexFile(const char *path, map<int,int> &mem)
{
    char abspath[PATH_MAX] = {};
    realpath(path, abspath);
    ifstream in(abspath);
    string str;
    int extBase = 0;              // from type-04 extended linear address (bytes)

    while (getline(in, str)) {
        if (str.empty() || str[0] != ':') continue;
        const char *h = str.c_str();

        auto hx = [&](int off, int n) -> long {
            char t[9] = {};
            strncpy(t, h + off, n);
            return strtol(t, nullptr, 16);
        };

        int nbytes = (int)hx(1, 2);
        int addr   = (int)hx(3, 4);       // byte address (low 16 bits)
        int rtype  = (int)hx(7, 2);

        if (rtype == 0x04) {              // extended linear address
            extBase = (int)hx(9, 4) << 16;
            continue;
        }
        if (rtype == 0x01) break;         // EOF
        if (rtype != 0x00) continue;      // ignore others

        int byteAddr = extBase | addr;
        // PIC16 program memory lives in the low 16-bit space here; config space
        // (byteAddr >= 0x10000) is not flashed by the bootloader — skip it.
        if (byteAddr >= 0x10000) continue;

        // Intel HEX data is little-endian byte pairs -> 14-bit words.
        for (int i = 0; i < nbytes; i += 2) {
            int lo = (int)hx(9 + i*2,     2);
            int hi = (int)hx(9 + i*2 + 2, 2);
            int word = ((hi << 8) | lo) & 0x3FFF;
            int waddr = (byteAddr + i) / 2;
            mem[waddr] = word;
        }
    }
}

void fwConvertPic16F1xxx(char* inFilename, char* outFilename)
{
    map<int,int> mem;
    parseIntelHexFile(inFilename, mem);

    char outAbs[PATH_MAX] = {};
    realpath(outFilename, outAbs);
    ofstream out(outAbs, ios_base::trunc);
    if (mem.empty()) { out.close(); return; }

    // Collect the set of rows (0x20-aligned) that contain any programmed word.
    // std::map keeps addresses sorted, so we can walk rows in order.
    int lastWord = mem.rbegin()->first;
    int firstWord = mem.begin()->first;
    int firstRow = firstWord & ~(ROW_WORDS - 1);
    int lastRow  = lastWord  & ~(ROW_WORDS - 1);

    for (int row = firstRow; row <= lastRow; row += ROW_WORDS) {
        // Does this row contain any programmed word?
        bool anyData = false;
        for (int w = 0; w < ROW_WORDS; w++) {
            if (mem.count(row + w)) { anyData = true; break; }
        }
        if (!anyData) continue;

        // Emit one full aligned block: LL CC AAAA + 64 data bytes.
        char hdr[16] = {};
        sprintf(hdr, "%02X%02X%04X", 4 + ROW_WORDS*2, WRITE_TO_MEM, row);
        out.write(hdr, 8);

        for (int w = 0; w < ROW_WORDS; w++) {
            int word = mem.count(row + w) ? mem[row + w] : BLANK_WORD;
            char wh[8] = {};
            sprintf(wh, "%02X%02X", (word >> 8) & 0xFF, word & 0xFF); // big-endian H,L
            out.write(wh, 4);
        }
        out.write("\n", 1);
    }
    out.close();
}
