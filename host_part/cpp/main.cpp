#include <unistd.h>
#include <limits.h>
#include <vector>
#include "fw_converter.h"
#include "uart_procedures.h"
#include "deviceconfig.h"
#include "imagev2.h"

using namespace std;

#define MAX_BYTES_TO_SEND   68
#define MAX_BYTES_TO_RECV   66

// ---------------------------------------------------------------------------
// Frame / protocol layout constants (replaces magic numbers throughout main).
//
// Converter output line (fwConvertPic16F1xxx), all ASCII-hex, no ':' :
//     LL CC AAAA DDDD...
//   char offset 0  : LL   frame length byte      (2 hex chars)
//   char offset 2  : CC   command byte           (2 hex chars)
//   char offset 4  : AAAA word address           (4 hex chars)
//   char offset 8  : data bytes begin            (2 hex chars per byte)
//
// A frame sent to the MCU is: send_buf[0]=len, [1]=cmd, [2]=addrH, [3]=addrL,
// then up to 64 data bytes at send_buf[4..67] (32 words, big-endian).
// A read response is: read_buf[0]=len, [1]=cmd, then 64 data bytes at [2..65].
// ---------------------------------------------------------------------------
#define HEX_OFF_LEN         0      // char offset of LL in a converter line
#define HEX_OFF_CMD         2      // char offset of CC
#define HEX_OFF_ADDR        4      // char offset of AAAA
#define HEX_OFF_DATA        8      // char offset of first data byte

#define FRAME_HDR_BYTES     4      // len + cmd + addrH + addrL
#define FRAME_FULL_LEN      0x44   // length byte value for a full 64-byte block
#define BLOCK_DATA_BYTES    64     // 32 words per flash row
#define BLOCK_DATA_WORDS    32

#define READ_DATA_OFFSET    2      // data begins at read_buf[2] (after len,cmd)

// Flash memory layout. The full layout comes from the device profile at
// runtime (DeviceConfig); only the reset-row address is needed as a literal
// here (to skip the bootloader's trampoline words during read-back verify).
#define ADDR_RESET          0x0000 // reset vector row (words 0x0000-0x0003 = BL trampoline)



char serial_name[32] = {};
char serial_speed[6] = {};
char inFilename[64] = {};
char saveFwFilename[64] = {};
char eepromFilename[64] = {};
char genImageIn[64] = {};
char genImageOut[64] = {};
char deviceName[32] = "16f1789";   // default device profile (-c overrides)
int param_count = 0;
short verbose = 0;
short start_app = 0;
short read_app = 0;
short flashup_app = 0;
short eeprom_write = 0;
short gen_image = 0;
short arm_extupgrade = 0;
unsigned int extUpgradeAddr = 0;
short eeprom_read = 0;
char eepromReadFile[64] = {};
unsigned int eeReadAddr = 0;
long eeReadLen = -1;               // -1 = auto (from image header)
short goto_bl = 0;                 // --goto-bl: ask a running app to enter the BL
bool isDeviceFound = false;
unsigned int foundDeviceId = 0;    // DEVID read from the connected chip
DeviceConfig devcfg;               // loaded device profile

int serialPort_fd = 0;
int received_bytes = 0;
char read_buf[MAX_BYTES_TO_RECV] = {};
char send_buf[MAX_BYTES_TO_SEND] = {};
ofstream saveFwFilenameFd;
string str;
char saveFwFilenameAbsolutePath[64] = {};
int lineAddr = 0;


// Send one frame and read the response back into read_buf.
// Returns the number of received bytes (>=0), or -1 on timeout/no preamble.
// Centralises the send+recv pattern that was duplicated across main().
int Transact(char *frame){
    UART_Send(serialPort_fd, frame, frame[0]);
    return UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);
}

// Like Transact(), but aborts the process if a full frame was not received.
// Mirrors the original inline "(received_bytes == -1) | (< MAX_BYTES_TO_RECV)"
// checks that ended in exit(6).
int TransactExpectFull(char *frame, const char *ctx){
    int n = Transact(frame);
    if (n == -1 || n < MAX_BYTES_TO_RECV) {
        printf("\n[UART][RECEIVE] ERROR (%s) Wrong number of bytes or corrupted frame was received! (%i)\n", ctx, n);
        exit(6);
    }
    return n;
}

// Send a WRITE frame and require the MCU to answer with SUCCESS_CODE.
// Aborts on comms error or a non-success reply. Returns true on success.
bool TransactWriteExpectAck(char *frame, const char *ctx){
    int n = Transact(frame);
    if (n == -1) {
        printf("\n[UART][RECEIVE] ERROR (%s) no/broken response.\n", ctx);
        exit(6);
    }
    if (read_buf[1] != (char)SUCCESS_CODE) {
        printf("\n[UART][RECEIVE] ERROR (%s) device reported unsuccessful code 0x%02hhX.\n", ctx, read_buf[1]);
        exit(6);
    }
    return true;
}

// Build a 4-byte READ_FROM_MEM request for the given 16-bit address.
void MakeReadRequest(char *frame, int addr){
    memset(frame, 0, MAX_BYTES_TO_SEND);
    frame[0] = 0x04;                 // frame length (incl. this byte)
    frame[1] = READ_FROM_MEM;
    frame[2] = (char)((addr >> 8) & 0xFF);
    frame[3] = (char)(addr & 0xFF);
}



bool FoundDevice(){

printf("[UART][SEND] Trying to found device ... \n");

//======= Ping to device =====================================================================================
    memset (send_buf, 0, sizeof(send_buf));
    send_buf[0] = 0x04;    // Length of data in frame, include this byte also
    send_buf[1] = READ_FROM_MEM;
    send_buf[2] = 0x80;
    send_buf[3] = 0x06;

    UART_Send(serialPort_fd, send_buf, send_buf[0]);
    received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);
    if (received_bytes == -1) {
        printf("     ERROR Device not found!\n");
        return false;
    } else {
            if (read_buf[0] == 0x04 && read_buf[1] == (char)READ_FROM_MEM){
                foundDeviceId = (((unsigned char)read_buf[2]) << 8) | (unsigned char)read_buf[3];
                printf("             Device ID: %hhX.%hhX\n", read_buf[2], read_buf[3]);
                send_buf[3] = 0x05;
                UART_Send(serialPort_fd, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);
                printf("             Device revision: %hhX.%hhX\n", read_buf[2], read_buf[3]);
                return true;
            }else{
                printf("ERROR in response! %hhX  %hhX\n", read_buf[0], read_buf[1]);
                return false;
           }
            }
    
}


int main(int argc, char** argv) {
printf("Microchip firmware uploader v1.3 (image format v2)\n");

	for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
        		printf("Cli arguments keys: \n");
        		printf("     -D     Port name (e.g. /dev/ttyUSB0)\n");
        		printf("     -b     Port speed (e.g. 9600)\n");
        		printf("     -f     Path to HEX file with firmware\n");     
                printf("     -s     Start new application after upgrade\n");
                printf("     -r     Just read fw from MCU\n");        		
                printf("     -e     Write a firmware image into the external EEPROM\n");
                printf("     -g in.hex out.img   Generate a firmware image (offline)\n");
                printf("     -E file [addr] [nbytes]  Read the external EEPROM to a file\n");
                printf("            (addr default 0; nbytes default = auto from image header)\n");
                printf("     -u [addr]  Arm autonomous EEPROM upgrade at EEPROM addr (default 0) and reset\n");
                printf("     --goto-bl  Ask the RUNNING application to enter the bootloader (no RB0 needed)\n");
                printf("     -c     Device profile name (default 16f1789)\n");
        		printf("     -v     Verbose mode\n");  
        		return 1;
        	}

        if (strcmp(argv[i], "-D") == 0) {
        	if (argv[i+1]) {
        		strcpy(serial_name, argv[i+1]);
        		++param_count;
        	} else {
        		printf("Define serial port name.\n");
        		return 1;
        	}
        }

        if (strcmp(argv[i], "-b") == 0) {
        	if (argv[i+1]) {
        		strcpy(serial_speed, argv[i+1]);
        		++param_count;
        	} else {
        		printf("Define serial port speed.\n");
        		return 1;
        	}
        }

        if (strcmp(argv[i], "-f") == 0) {
        	flashup_app = 1;
            if (argv[i+1]) {
        		strcpy(inFilename, argv[i+1]);
        		++param_count;
        	} else {
        		printf("Define path to firmware in HEX format.\n");
        		return 1;
        	}
        }

        if (strcmp(argv[i], "-v") == 0) {
        	verbose = 1;
        }

        if (strcmp(argv[i], "--goto-bl") == 0) {
            goto_bl = 1;
            ++param_count;
        }

        if (strcmp(argv[i], "-c") == 0) {
            if (argv[i+1]) {
                strcpy(deviceName, argv[i+1]);
            } else {
                printf("Define a device profile name (e.g. 16f1789).\n");
                return 1;
            }
        }

        if (strcmp(argv[i], "-u") == 0) {
            arm_extupgrade = 1;
            ++param_count;
            // Optional next arg = EEPROM address (decimal or 0x-hex). If the next
            // token starts with '-' (another flag) or is absent, default to 0.
            if (argv[i+1] && argv[i+1][0] != '-') {
                extUpgradeAddr = (unsigned int)strtoul(argv[i+1], nullptr, 0);
            }
        }

         if (strcmp(argv[i], "-s") == 0) {
            ++param_count;
            start_app = 1;
        }       

         if (strcmp(argv[i], "-r") == 0) {
            read_app = 1;
            if (argv[i+1]) {
                strcpy(saveFwFilename, argv[i+1]);
                ++param_count;
            } else {
                printf("Define path to save MCU fw.\n");
                return 1;
            }
        }   

         if (strcmp(argv[i], "-e") == 0) {
            eeprom_write = 1;
            if (argv[i+1]) {
                strcpy(eepromFilename, argv[i+1]);
                ++param_count;
            } else {
                printf("Define path to the binary file to write into EEPROM.\n");
                return 1;
            }
        }

         if (strcmp(argv[i], "-g") == 0) {
            gen_image = 1;
            if (argv[i+1] && argv[i+2]) {
                strcpy(genImageIn,  argv[i+1]);
                strcpy(genImageOut, argv[i+2]);
            } else {
                printf("Usage: -g <in.hex> <out.img>\n");
                return 1;
            }
        }

         if (strcmp(argv[i], "-E") == 0) {
            eeprom_read = 1;
            ++param_count;
            if (argv[i+1] && argv[i+1][0] != '-') {
                strcpy(eepromReadFile, argv[i+1]);
            } else {
                printf("Usage: -E <file> [addr] [nbytes]\n");
                return 1;
            }
            // Optional addr and nbytes (decimal or 0x-hex), both start non-'-'.
            if (argv[i+2] && argv[i+2][0] != '-') {
                eeReadAddr = (unsigned int)strtoul(argv[i+2], nullptr, 0);
                if (argv[i+3] && argv[i+3][0] != '-')
                    eeReadLen = (long)strtoul(argv[i+3], nullptr, 0);
            }
        }
    }

// Load the device profile (needed by both offline image generation and flashing).
if (LoadDeviceConfig(deviceName, &devcfg) != 0) {
    return 1;
}
printf("Device profile: %s (device_id=0x%04X, flash=%u words, row=%u)\n",
       devcfg.name, devcfg.device_id, devcfg.flash_words, devcfg.row_words);

// -g is an offline operation (no device): generate a firmware image and exit.
if (gen_image == 1) {
    printf("Generating firmware image: %s -> %s\n", genImageIn, genImageOut);
    int rc = writeImageV2File(genImageIn, genImageOut, &devcfg);
    if (rc != 0) { printf("ERROR: image generation failed (%d)\n", rc); return 1; }
    printf("Done.\n");
    return 0;
}


if (param_count < 3) {
	printf("\nERROR Not enough parameters to continue.\n");
	return 1;
}


printf("Connecting to %s, %s\n", serial_name, serial_speed);
serialPort_fd = UART_Init(serial_name, serial_speed);
UART_Clear(serialPort_fd);

// --goto-bl: ask the RUNNING application to hand over to the bootloader, so no
// RB0 button press is needed. The application ACKs, sets IsBLStart and resets;
// the bootloader then comes up in its command loop. If the device is ALREADY in
// the bootloader it answers ERROR to this (unknown to it) command — harmless,
// we just carry on.
if (goto_bl == 1) {
    printf("[UART][SEND] Requesting the application to enter the bootloader ...\n");

    // Retry: an application polls its UART in between doing real work, and the
    // receiver FIFO is only 2 bytes deep, so a single request can be missed.
    // Several attempts make this reliable without the application needing an
    // interrupt-driven receiver.
    const int GOTO_BL_TRIES = 8;
    int  gotoAck = 0, gotoRefused = 0;
    for (int attempt = 1; attempt <= GOTO_BL_TRIES; attempt++) {
        memset(send_buf, 0, sizeof(send_buf));
        send_buf[0] = 0x02;
        send_buf[1] = ENTER_BOOTLOADER;
        UART_Send(serialPort_fd, send_buf, send_buf[0]);
        received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);

        if (received_bytes >= 2 && read_buf[1] == (char)SUCCESS_CODE) {
            printf("[UART][RECV] ACK on attempt %d — application is resetting into the bootloader.\n",
                   attempt);
            gotoAck = 1;
            break;
        }
        if (received_bytes >= 2 && read_buf[1] == (char)ERROR_CODE) {
            gotoRefused = 1;
            break;
        }
        UART_Clear(serialPort_fd);      // drop partial/garbage, then retry
    }

    if (gotoRefused) {
        printf("[UART][RECV] command refused — the device is most likely already\n");
        printf("             in the bootloader. Continuing.\n");
    } else if (!gotoAck) {
        printf("[UART][RECV] no answer after %d attempts. Either the application does\n", GOTO_BL_TRIES);
        printf("             not implement ENTER_BOOTLOADER, or it is already in the\n");
        printf("             bootloader. Continuing (use RB0 + reset if detection fails).\n");
    }

    usleep(400000);                 // let the MCU reset and the bootloader start
    UART_Clear(serialPort_fd);      // drop anything left from the reset
}

isDeviceFound = FoundDevice();

if (!isDeviceFound){
    return 1;
}

// Guard against flashing the wrong binary: the profile's device_id must match
// the DEVID read from the connected chip (unless the profile is a wildcard).
if (devcfg.device_id != IMGV2_DEVICE_ANY && foundDeviceId != devcfg.device_id) {
    printf("\nERROR: device mismatch — profile '%s' expects 0x%04X but chip reports 0x%04X.\n",
           devcfg.name, devcfg.device_id, foundDeviceId);
    printf("Use the correct -c profile (or a wildcard profile) to proceed.\n");
    return 1;
}

if (flashup_app == 1){

printf("Firmware %s\n\n", inFilename);

// Build the unified image blocks for this device (same code path as -g).
vector<ImageBlock> blocks;
string err;
if (buildImageV2Blocks(inFilename, &devcfg, blocks, err) != 0) {
    printf("ERROR building image: %s\n", err.c_str());
    return 1;
}

for (const ImageBlock &blk : blocks) {
    lineAddr = (int)blk.addr;

    // Assemble the WRITE_TO_MEM frame: len, cmd, addrH, addrL + 64 data bytes.
    memset(send_buf, 0, MAX_BYTES_TO_SEND);
    send_buf[0] = FRAME_FULL_LEN;
    send_buf[1] = WRITE_TO_MEM;
    send_buf[2] = (char)((blk.addr >> 8) & 0xFF);
    send_buf[3] = (char)(blk.addr & 0xFF);
    for (int b = 0; b < BLOCK_DATA_BYTES; b++)
        send_buf[FRAME_HDR_BYTES + b] = (char)blk.data[b];

                printf("\n[UART][SEND] Write to addr 0x%04X ... ", lineAddr);

                // Snapshot the 64 data bytes to verify after read-back.
                char written_data[64] = {};
                for (int s = 0; s < 64; s++) {
                    written_data[s] = send_buf[s + FRAME_HDR_BYTES];
                }

                UART_Send(serialPort_fd, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1)) {
                    printf("\n[UART][RECEIVE] ERROR Wrong number of bytes or corrupted frame was received!\n");
                    exit(6);
                    }
                printf(" SUCCESS.\n");           




//======================================================================================
printf("\n============= Read/verify at 0x%04X ...              ", lineAddr);

            // Read the block back from the SAME address and compare with what we
            // wrote (verify). Costs nothing on the MCU side - reuses READ_FROM_MEM.
            MakeReadRequest(send_buf, lineAddr);
            received_bytes = TransactExpectFull(send_buf, "verify read-back");

            {
                // Flash is 14-bit: the top 2 bits read back as 0, so mask each
                // word with 0x3FFF before comparing. read_buf[2..65] = 64 bytes.
                // For row 0 the bootloader intentionally keeps words 0-3 (its own
                // reset trampoline) instead of our app data, so skip those words.
                int first_word = (lineAddr == ADDR_RESET) ? (int)devcfg.reset_vector_words : 0;
                int mismatches = 0;
                for (int k = first_word; k < 32; k++) {
                    int wrote = (((unsigned char)written_data[k*2] << 8)
                                 | (unsigned char)written_data[k*2 + 1]) & 0x3FFF;
                    int got   = (((unsigned char)read_buf[2 + k*2] << 8)
                                 | (unsigned char)read_buf[3 + k*2]) & 0x3FFF;
                    if (wrote != got) {
                        if (mismatches == 0) printf("\n");
                        printf("[VERIFY] MISMATCH at word 0x%04X: wrote %04X, read %04X\n",
                               lineAddr + k, wrote, got);
                        mismatches++;
                    }
                }
                if (mismatches) {
                    printf("[VERIFY] FAILED at block 0x%04X (%i word(s) differ)\n",
                           lineAddr, mismatches);
                    exit(7);
                }
            }
            printf(" VERIFIED.\n");
//=======================================================================================

    }   // end for(blocks) — next image block


printf("\n============= Read from 0x0000\n");


            MakeReadRequest(send_buf, 0x0000);
            received_bytes = TransactExpectFull(send_buf, "dump 0x0000");
 
            for (int s=2; s < (int)sizeof(read_buf); s+=2){
                 printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            }
            printf("\n SUCCESS.\n");

printf("\n============= Read from 0x3FE0\n");


            MakeReadRequest(send_buf, 0x3FE0);
            received_bytes = TransactExpectFull(send_buf, "dump 0x3FE0");
 
            for (int s=2; s < (int)sizeof(read_buf); s+=2){
                 printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            }
            printf("\n SUCCESS.\n");



printf("[FW] Flasing done.\n");


}




if (read_app == 1){

printf("\n[FW] Saving firmware to %s\n", saveFwFilename);
realpath(saveFwFilename, saveFwFilenameAbsolutePath);
saveFwFilenameFd.open(saveFwFilenameAbsolutePath);

printf("[UART][SEND] Reading MCU memory ...\n");

char temp2[7] = {};
memset (temp2, 0, sizeof(temp2));


memset (send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x04;    // Length of data in frame, include this byte also
send_buf[1] = READ_FROM_MEM;

char strr[] = "[UART][READ]";
for (int p = 0x0; p < 0x3FFF; p += 0x20){
            ProgressBar(strr, 0x0, 0x3FFF, p);
            //printf("\r[UART][READ] Reading addr > %02hhX%02hhX", ((p & 0xFF00)>>8), (p & 0x00FF));

                MakeReadRequest(send_buf, p);
                received_bytes = TransactExpectFull(send_buf, "read loop");
            //for (int s=2; s < (int)sizeof(read_buf); s+=2){
            //     printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            //}

            sprintf(temp2, "\n%02X%02X ", ((p & 0xFF00)>>8), (p & 0x00FF));
            saveFwFilenameFd.write(temp2, 6);

            for (int s=2; s < (int)sizeof(read_buf); s++){

                sprintf(temp2, "%02hhX", read_buf[s]);
                saveFwFilenameFd.write(temp2, 2);

            }


            saveFwFilenameFd.flush();
}
            printf("\n[FW] Reading done\n");


}


if (eeprom_write == 1){

printf("\n[EEPROM] Writing raw dump from %s\n", eepromFilename);

char eepromAbsPath[PATH_MAX] = {};
realpath(eepromFilename, eepromAbsPath);
FILE *ef = fopen(eepromAbsPath, "rb");
if (!ef) {
    printf("[EEPROM] ERROR: cannot open %s\n", eepromFilename);
    return 1;
}

// The external 25LC512 is byte-addressed, 64 KiB (0x0000-0xFFFF). We write it
// in BLOCK_DATA_BYTES (64-byte) chunks — the same block size the MCU write
// command expects. Blocks are placed on 64-byte boundaries; two 64-byte blocks
// share a 128-byte EEPROM page but do not clobber each other (Page Erase was
// removed from the firmware, so the page "refresh" preserves the neighbour).
unsigned char filebuf[BLOCK_DATA_BYTES];
int eeAddr = 0;
size_t nread = 0;
int blocks = 0;

while ((nread = fread(filebuf, 1, BLOCK_DATA_BYTES, ef)) > 0) {
    if (eeAddr + (int)nread > 0x10000) {
        printf("[EEPROM] ERROR: data exceeds 64 KiB EEPROM capacity at 0x%04X\n", eeAddr);
        fclose(ef); return 1;
    }
    // Pad a short final block with 0xFF (erased value).
    for (size_t k = nread; k < BLOCK_DATA_BYTES; k++) filebuf[k] = 0xFF;

    // Build WRITE_TO_SERIAL_EEPROM frame: len, cmd, addrH, addrL + 64 data.
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = FRAME_FULL_LEN;                 // 0x44
    send_buf[1] = WRITE_TO_SERIAL_EEPROM;
    send_buf[2] = (char)((eeAddr >> 8) & 0xFF);
    send_buf[3] = (char)(eeAddr & 0xFF);
    for (int b = 0; b < BLOCK_DATA_BYTES; b++)
        send_buf[FRAME_HDR_BYTES + b] = (char)filebuf[b];

    printf("[EEPROM][SEND] Write 0x%04X ... ", eeAddr);
    TransactWriteExpectAck(send_buf, "eeprom write");

    // Verify: read the block back (0x12) and compare.
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = 0x04;
    send_buf[1] = READ_FROM_SERIAL_EEPROM;
    send_buf[2] = (char)((eeAddr >> 8) & 0xFF);
    send_buf[3] = (char)(eeAddr & 0xFF);
    TransactExpectFull(send_buf, "eeprom verify");
    int mism = 0;
    for (int b = 0; b < BLOCK_DATA_BYTES; b++)
        if ((unsigned char)read_buf[READ_DATA_OFFSET + b] != filebuf[b]) mism++;
    if (mism) {
        printf("\n[EEPROM] VERIFY FAILED at 0x%04X (%d byte(s) differ)\n", eeAddr, mism);
        fclose(ef); return 1;
    }
    printf("VERIFIED.\n");

    eeAddr += BLOCK_DATA_BYTES;
    blocks++;
}
fclose(ef);
printf("[EEPROM] Done: %d block(s), %d bytes written & verified.\n",
       blocks, blocks * BLOCK_DATA_BYTES);

}


if (eeprom_read == 1){

// Determine how many bytes to read. If nbytes was not given, auto-detect from
// the image-v2 header at the start address: read the 64-byte header, and if it
// carries the MPFU magic, compute the exact length = header + block_count*66.
long toRead = eeReadLen;
if (toRead < 0) {
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = 0x04;
    send_buf[1] = READ_FROM_SERIAL_EEPROM;
    send_buf[2] = (char)((eeReadAddr >> 8) & 0xFF);
    send_buf[3] = (char)(eeReadAddr & 0xFF);
    TransactExpectFull(send_buf, "eeprom header read");
    unsigned char *h = (unsigned char*)&read_buf[READ_DATA_OFFSET];
    if (h[0]=='M' && h[1]=='P' && h[2]=='F' && h[3]=='U') {
        unsigned int block_count = (h[7] << 8) | h[8];   // IMGV2_OFF_BLOCK_COUNT
        toRead = 64 + (long)block_count * 66;            // header + N*66
        printf("[EEPROM] Auto length from image header: %u block(s) -> %ld bytes\n",
               block_count, toRead);
    } else {
        printf("[EEPROM] ERROR: no MPFU image at 0x%04X and no nbytes given.\n", eeReadAddr);
        printf("         Specify a length: -E %s 0x%04X <nbytes>\n", eepromReadFile, eeReadAddr);
        return 1;
    }
}

char eeOutAbs[PATH_MAX] = {};
realpath(eepromReadFile, eeOutAbs);
FILE *of = fopen(eeOutAbs[0] ? eeOutAbs : eepromReadFile, "wb");
if (!of) { printf("[EEPROM] ERROR: cannot open %s for writing\n", eepromReadFile); return 1; }

printf("[EEPROM] Reading %ld bytes from 0x%04X into %s ...\n", toRead, eeReadAddr, eepromReadFile);
long done = 0;
unsigned int addr = eeReadAddr;
while (done < toRead) {
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = 0x04;
    send_buf[1] = READ_FROM_SERIAL_EEPROM;
    send_buf[2] = (char)((addr >> 8) & 0xFF);
    send_buf[3] = (char)(addr & 0xFF);
    TransactExpectFull(send_buf, "eeprom read");

    long chunk = toRead - done;
    if (chunk > BLOCK_DATA_BYTES) chunk = BLOCK_DATA_BYTES;   // 64B blocks
    fwrite(&read_buf[READ_DATA_OFFSET], 1, (size_t)chunk, of);
    done += chunk;
    addr += BLOCK_DATA_BYTES;
}
fclose(of);
printf("[EEPROM] Done: %ld bytes written to %s.\n", done, eepromReadFile);

}


if (arm_extupgrade == 1){
printf("[UART][SEND] Arming ExtUpgrade at EEPROM 0x%04X (device will reset)...\n", extUpgradeAddr);
memset(send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x04;
send_buf[1] = SET_EXT_UPGRADE;
send_buf[2] = (char)((extUpgradeAddr >> 8) & 0xFF);
send_buf[3] = (char)(extUpgradeAddr & 0xFF);
// The MCU ACKs, then resets and runs the upgrade autonomously — so we send and
// read the single ACK, but tolerate a truncated/absent reply (reset races it).
UART_Send(serialPort_fd, send_buf, send_buf[0]);
received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);
if (received_bytes >= 2 && read_buf[1] == (char)SUCCESS_CODE) {
    printf("[UART][RECV] ACK — device is upgrading from EEPROM, then starting the app.\n");
} else {
    printf("[UART][RECV] no clean ACK (device likely already reset). Check the board.\n");
}

}


if (start_app == 1){
printf("[UART][SEND] Starting App ...\n");
memset (send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x02;    // Length of data in frame, include this byte also
send_buf[1] = START_APPLICATION;
UART_Send(serialPort_fd, send_buf, send_buf[0]);

}

       

return 1; //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<,

}


