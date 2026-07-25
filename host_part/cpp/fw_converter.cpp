#include "fw_converter.h"
#include "imagev2.h"
#include <map>
#include <limits.h>
using namespace std;

// ---------------------------------------------------------------------------
// Intel HEX -> unified image v2 (sparse list of aligned flash blocks).
//
// All platform-specific decisions live here (host side), driven by a
// DeviceConfig. The bootloader stays a dumb writer: it just programs each
// {addr, 32 words} block, enforcing only its own code/reset protection.
// ---------------------------------------------------------------------------

#define BLANK_FALLBACK 0x3FFF

// Parse an Intel HEX file into a word map (word address -> 14-bit word).
static void parseIntelHexFile(const char *path, map<int,int> &mem)
{
    char abspath[PATH_MAX] = {};
    realpath(path, abspath);
    ifstream in(abspath[0] ? abspath : path);
    string str;
    int extBase = 0;

    while (getline(in, str)) {
        if (str.empty() || str[0] != ':') continue;
        const char *h = str.c_str();

        auto hx = [&](int off, int n) -> long {
            char t[9] = {};
            strncpy(t, h + off, n);
            return strtol(t, nullptr, 16);
        };

        int nbytes = (int)hx(1, 2);
        int addr   = (int)hx(3, 4);
        int rtype  = (int)hx(7, 2);

        if (rtype == 0x04) { extBase = (int)hx(9, 4) << 16; continue; }
        if (rtype == 0x01) break;      // EOF
        if (rtype != 0x00) continue;   // ignore others

        int byteAddr = extBase | addr;
        if (byteAddr >= 0x10000) continue;   // config space: not flashed here

        for (int i = 0; i < nbytes; i += 2) {
            int lo = (int)hx(9 + i*2,     2);
            int hi = (int)hx(9 + i*2 + 2, 2);
            int word = ((hi << 8) | lo) & 0x3FFF;
            int waddr = (byteAddr + i) / 2;
            mem[waddr] = word;
        }
    }
}

// ---- PIC16 enhanced-midrange opcode helpers -------------------------------
// MOVLP k : 0x3180 | (k & 0x7F)   -> sets PCLATH = k & 0x7F
// GOTO  a : 0x2800 | (a & 0x7FF)  -> PC = (PCLATH<7:3> << 11) | (a & 0x7FF)
static bool isMovlp(int w) { return (w & 0x3F80) == 0x3180; }
static bool isGoto (int w) { return (w & 0x3800) == 0x2800; }
static int  movlpVal(int w){ return w & 0x7F; }
static int  gotoField(int w){ return w & 0x7FF; }

// Resolve the application's real entry point by following its reset-vector jump
// chain. Each hop is a {MOVLP; GOTO} pair. We follow while the GOTO target lands
// inside the reserved trampoline words (< reset_vector_words), because those
// words get overwritten with the bootloader's own jump. When the target points
// past them (into untouched app memory), that's the real entry.
//
// Returns the entry word address, or -1 if the vector could not be decoded.
static int resolveAppEntry(map<int,int> &mem, const DeviceConfig *cfg)
{
    int pc = 0;
    for (int hops = 0; hops < 4; hops++) {   // guard against loops
        // Read the (up to) two words at pc: optional MOVLP then GOTO.
        int w0 = mem.count(pc)   ? mem[pc]   : cfg->blank_word;
        int w1 = mem.count(pc+1) ? mem[pc+1] : cfg->blank_word;

        int pclath = 0, target = -1;
        if (isMovlp(w0) && isGoto(w1)) {
            pclath = movlpVal(w0);
            target = ((pclath >> 3) << 11) | gotoField(w1);
        } else if (isGoto(w0)) {
            target = gotoField(w0);          // no MOVLP => page 0
        } else {
            return -1;                       // unrecognised reset vector
        }

        if ((unsigned)target >= cfg->reset_vector_words)
            return target;                   // real entry (past the trampoline)

        pc = target;                         // hop again through the trampoline
    }
    return -1;
}

// Build a row (32 words) into out[64] big-endian, pulling from mem, gaps blank.
static void fillRow(map<int,int> &mem, const DeviceConfig *cfg,
                    unsigned int row, unsigned char *out64)
{
    for (unsigned int w = 0; w < cfg->row_words; w++) {
        int word = mem.count(row + w) ? mem[row + w] : (int)cfg->blank_word;
        out64[w*2]     = (unsigned char)((word >> 8) & 0xFF);
        out64[w*2 + 1] = (unsigned char)(word & 0xFF);
    }
}

int buildImageV2Blocks(const char *inHexPath, const DeviceConfig *cfg,
                       vector<ImageBlock> &blocks, string &err)
{
    map<int,int> mem;
    parseIntelHexFile(inHexPath, mem);
    if (mem.empty()) { err = "no data in HEX file"; return 1; }

    const unsigned int ROW = cfg->row_words;

    // 1. Resolve the application's real entry point from its reset vector.
    int entry = resolveAppEntry(mem, cfg);
    if (entry < 0) { err = "could not decode application reset vector"; return 2; }

    // 2. Emit every populated row of the application, skipping the regions the
    //    bootloader owns (its code and its flags row). Row 0 IS emitted: the
    //    bootloader keeps words 0-3 (its trampoline) and takes 4-31 from us.
    int firstWord = mem.begin()->first;
    int lastWord  = mem.rbegin()->first;
    unsigned int firstRow = firstWord & ~(ROW - 1);
    unsigned int lastRow  = lastWord  & ~(ROW - 1);

    for (unsigned int row = firstRow; row <= lastRow; row += ROW) {
        if (row >= cfg->bl_code_start && row <= cfg->bl_code_end) continue; // BL code
        if (row == cfg->flags_row)      continue;                          // flags
        if (row == cfg->app_vector_row) continue;    // synthesised separately below

        bool anyData = false;
        for (unsigned int w = 0; w < ROW; w++)
            if (mem.count(row + w)) { anyData = true; break; }
        if (!anyData) continue;

        ImageBlock b; b.addr = row;
        fillRow(mem, cfg, row, b.data);
        blocks.push_back(b);
    }

    // 3. Synthesise the app-vector row: a canonical MOVLP;GOTO <entry> at the
    //    start of the row, rest blank. StartApp() in the bootloader jumps here.
    {
        ImageBlock b; b.addr = cfg->app_vector_row;
        for (unsigned int w = 0; w < ROW; w++) {
            b.data[w*2] = (unsigned char)((cfg->blank_word >> 8) & 0xFF);
            b.data[w*2 + 1] = (unsigned char)(cfg->blank_word & 0xFF);
        }
        int movlp = 0x3180 | ((entry >> 11) << 3 & 0x7F);
        int gotoi = 0x2800 | (entry & 0x7FF);
        b.data[0] = (unsigned char)((movlp >> 8) & 0xFF);
        b.data[1] = (unsigned char)(movlp & 0xFF);
        b.data[2] = (unsigned char)((gotoi >> 8) & 0xFF);
        b.data[3] = (unsigned char)(gotoi & 0xFF);
        blocks.push_back(b);
    }

    printf("[IMG] entry=0x%04X, %zu block(s) (incl. app-vector @0x%04X)\n",
           entry, blocks.size(), cfg->app_vector_row);
    return 0;
}

int serializeImageV2(const vector<ImageBlock> &blocks, unsigned int device_id,
                     vector<unsigned char> &out)
{
    // BLOCKS area first (so we can checksum it), then prepend the header.
    vector<unsigned char> body;
    body.reserve(blocks.size() * IMGV2_BLOCK_SIZE);
    for (const auto &b : blocks) {
        body.push_back((unsigned char)((b.addr >> 8) & 0xFF));
        body.push_back((unsigned char)(b.addr & 0xFF));
        for (int i = 0; i < IMGV2_BLOCK_DATA_BYTES; i++) body.push_back(b.data[i]);
    }

    unsigned short crc = fletcher16(body.data(), (unsigned int)body.size());

    unsigned char hdr[IMGV2_HEADER_SIZE];
    memset(hdr, 0xFF, sizeof(hdr));
    hdr[IMGV2_OFF_MAGIC + 0] = IMGV2_MAGIC0;
    hdr[IMGV2_OFF_MAGIC + 1] = IMGV2_MAGIC1;
    hdr[IMGV2_OFF_MAGIC + 2] = IMGV2_MAGIC2;
    hdr[IMGV2_OFF_MAGIC + 3] = IMGV2_MAGIC3;
    hdr[IMGV2_OFF_VERSION]      = IMGV2_FORMAT_VERSION;
    hdr[IMGV2_OFF_DEVICE_ID]    = (unsigned char)((device_id >> 8) & 0xFF);
    hdr[IMGV2_OFF_DEVICE_ID+1]  = (unsigned char)(device_id & 0xFF);
    hdr[IMGV2_OFF_BLOCK_COUNT]  = (unsigned char)((blocks.size() >> 8) & 0xFF);
    hdr[IMGV2_OFF_BLOCK_COUNT+1]= (unsigned char)(blocks.size() & 0xFF);
    hdr[IMGV2_OFF_CRC]          = (unsigned char)((crc >> 8) & 0xFF);
    hdr[IMGV2_OFF_CRC+1]        = (unsigned char)(crc & 0xFF);

    out.clear();
    out.insert(out.end(), hdr, hdr + IMGV2_HEADER_SIZE);
    out.insert(out.end(), body.begin(), body.end());
    return 0;
}

int writeImageV2File(const char *inHexPath, const char *outImgPath,
                     const DeviceConfig *cfg)
{
    vector<ImageBlock> blocks;
    string err;
    if (buildImageV2Blocks(inHexPath, cfg, blocks, err) != 0) {
        fprintf(stderr, "[IMG] ERROR: %s\n", err.c_str());
        return 1;
    }
    vector<unsigned char> img;
    serializeImageV2(blocks, cfg->device_id, img);

    char outAbs[PATH_MAX] = {};
    realpath(outImgPath, outAbs);
    FILE *of = fopen(outAbs[0] ? outAbs : outImgPath, "wb");
    if (!of) { fprintf(stderr, "[IMG] ERROR: cannot open %s\n", outImgPath); return 2; }
    fwrite(img.data(), 1, img.size(), of);
    fclose(of);

    printf("[IMG] wrote %s: %zu bytes (%zu blocks, device_id=0x%04X, v%d)\n",
           outImgPath, img.size(), blocks.size(), cfg->device_id, IMGV2_FORMAT_VERSION);
    return 0;
}
