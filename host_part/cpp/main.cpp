#include <unistd.h>
#include "fw_converter.h"
#include "uart_procedures.h"

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

// Flash memory layout (see agent-notes / docs/BL_memory.txt)
#define ADDR_RESET          0x0000 // reset vector row (words 0x0000-0x0003 = BL jump)
#define ADDR_FLAGS          0x3FE0 // bootloader flags row (0x3FE0-0x3FFF)
#define ADDR_APP_VECTOR     0x3FFC // app reset vector is relocated here (in flags row)
#define RESET_VECTOR_WORDS  4      // words 0x0000-0x0003 reserved for GOTO bootloader
#define RESET_VECTOR_BYTES  8      // 4 words * 2 bytes
// byte offset of the app-vector words within the flags-row data (0x3FFC-0x3FE0=28 words)
#define APP_VECTOR_BYTE_OFFSET ((ADDR_APP_VECTOR - ADDR_FLAGS) * 2)



char serial_name[32] = {};
char serial_speed[6] = {};
char inFilename[64] = {};
char outFilename[64] = {};
char saveFwFilename[64] = {};
int param_count = 0;
short verbose = 0;
short start_app = 0;
short read_app = 0;
short flashup_app = 0;
bool isDeviceFound = false;

int serialPort_fd = 0;
int received_bytes = 0;
char read_buf[MAX_BYTES_TO_RECV] = {};
char send_buf[MAX_BYTES_TO_SEND] = {};
ifstream convertedFwFileFd;
ofstream saveFwFilenameFd;
string str;
char hex_buffer[43] = {};
char outFilenameAbsolutePath[64] = {};
char saveFwFilenameAbsolutePath[64] = {};
char tmp_buffer[4+1] = {};
int lineAddr = 0;
int lineBytesNum = 0;
int tmp = 0;


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

// Parse two ASCII-hex chars at hex_line[charOffset] into a byte value.
int HexByteAt(const char *hex_line, int charOffset){
    char tmp[3] = {};
    strncpy(tmp, hex_line + charOffset, 2);
    return (int)strtol(tmp, nullptr, 16);
}

// Parse the 4 ASCII-hex chars of the word address (AAAA) into an int.
int HexAddrOf(const char *hex_line){
    char tmp[5] = {};
    strncpy(tmp, hex_line + HEX_OFF_ADDR, 4);
    return (int)strtol(tmp, nullptr, 16);
}

// Fill a full WRITE_TO_MEM frame (len,cmd,addr + 64 data bytes) from a converter
// line. The converter guarantees every line is a full 0x20-aligned 32-word block,
// so this is a straight copy of all 68 bytes.
void BuildWriteFrameFromLine(char *frame, const char *hex_line){
    for (int b = 0; b < MAX_BYTES_TO_SEND; b++)
        frame[b] = (char)HexByteAt(hex_line, b * 2);
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
printf("Microchip firmware uploader v1.1\n");
    strcpy(outFilename, "./last_fw.cof");

	for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
        		printf("Cli arguments keys: \n");
        		printf("     -D     Port name (e.g. /dev/ttyUSB0)\n");
        		printf("     -b     Port speed (e.g. 9600)\n");
        		printf("     -f     Path to HEX file with firmware\n");     
                printf("     -s     Start new application after upgrade\n");
                printf("     -r     Just read fw from MCU\n");        		
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
    }



if (param_count < 3) {
	printf("\nERROR Not enough parameters to continue.\n");
	return 1;
}


printf("Connecting to %s, %s\n", serial_name, serial_speed);
serialPort_fd = UART_Init(serial_name, serial_speed);
UART_Clear(serialPort_fd);

isDeviceFound = FoundDevice();

if (!isDeviceFound){
    return 1;
}

if (flashup_app == 1){

printf("Firmware %s\n\n", inFilename);
fwConvertPic16F1xxx(inFilename, outFilename);
printf("Converted firmware %s\n\n", outFilename);
realpath(outFilename, outFilenameAbsolutePath);
convertedFwFileFd.open(outFilenameAbsolutePath);

while (getline(convertedFwFileFd, str)) {

    memset (hex_buffer, 0, sizeof(hex_buffer));
    strcpy(hex_buffer, str.c_str()); //char * strcpy( char * destptr, const char * srcptr );

    lineAddr     = HexAddrOf(hex_buffer);              // AAAA word address
    lineBytesNum = HexByteAt(hex_buffer, HEX_OFF_LEN); // LL frame length byte


    switch (lineAddr){
        case ADDR_RESET: {
            // Row 0 (0x0000-0x001F). The converter gives us a full aligned block
            // whose words 0-3 are the APPLICATION's reset vector. We must NOT let
            // that overwrite the "GOTO bootloader" jump already programmed at
            // 0x0000-0x0003. So:
            //   1. read the current words 0x0000-0x0003 from the chip (the BL jump),
            //   2. build row 0 = [BL jump] + [app data 0x0004-0x001F],
            //   3. save the app's own reset vector (from the hex) into the flags
            //      row at ADDR_APP_VECTOR (0x3FFC), so the BL can start the app.
            printf("[FW] Row 0: preserving bootloader jump, relocating app reset vector.\n");

            // Keep the app's reset vector words (hex data bytes 0..7) for step 3.
            char app_reset[RESET_VECTOR_BYTES] = {};
            for (int b = 0; b < RESET_VECTOR_BYTES; b++)
                app_reset[b] = (char)HexByteAt(hex_buffer, HEX_OFF_DATA + b * 2);

            // Step 1: read current row 0 from the chip (BL jump lives in words 0-3).
            MakeReadRequest(send_buf, ADDR_RESET);
            TransactExpectFull(send_buf, "read row 0 (BL jump)");
            char bl_jump[RESET_VECTOR_BYTES] = {};
            for (int b = 0; b < RESET_VECTOR_BYTES; b++)
                bl_jump[b] = read_buf[READ_DATA_OFFSET + b];

            // Step 2: build row-0 write frame = app block, but words 0-3 = BL jump.
            BuildWriteFrameFromLine(send_buf, hex_buffer);
            for (int b = 0; b < RESET_VECTOR_BYTES; b++)
                send_buf[FRAME_HDR_BYTES + b] = bl_jump[b];

            printf("[UART][SEND] Write row 0 (BL jump + app 0x0004+) ... ");
            if (TransactWriteExpectAck(send_buf, "write row 0")) printf(" SUCCESS.\n");

            // Step 3: relocate the app reset vector into the flags row at 0x3FFC.
            printf("[FW] Storing app reset vector at 0x%04X (flags row).\n", ADDR_APP_VECTOR);
            MakeReadRequest(send_buf, ADDR_FLAGS);
            TransactExpectFull(send_buf, "read flags row");

            // Rebuild the flags row: keep existing contents, overwrite the last 4
            // words (0x3FFC-0x3FFF) with the app's saved reset vector.
            char flags_frame[MAX_BYTES_TO_SEND] = {};
            flags_frame[0] = FRAME_FULL_LEN;
            flags_frame[1] = WRITE_TO_MEM;
            flags_frame[2] = (char)((ADDR_FLAGS >> 8) & 0xFF);
            flags_frame[3] = (char)(ADDR_FLAGS & 0xFF);
            for (int b = 0; b < BLOCK_DATA_BYTES; b++)
                flags_frame[FRAME_HDR_BYTES + b] = read_buf[READ_DATA_OFFSET + b];
            for (int b = 0; b < RESET_VECTOR_BYTES; b++)
                flags_frame[FRAME_HDR_BYTES + APP_VECTOR_BYTE_OFFSET + b] = app_reset[b];

            printf("[UART][SEND] Write flags row with app reset vector ... ");
            if (TransactWriteExpectAck(flags_frame, "write flags row")) printf(" SUCCESS.\n");
            break;
        }
        default:
            // Every converter line is a full 0x20-aligned 32-word block, so just
            // copy it straight into the write frame.
            BuildWriteFrameFromLine(send_buf, hex_buffer);

//======================================================================================
printf("\n============= Write to 0x%04X, bytes=%i ... ", lineAddr, lineBytesNum);
            for (int s=0; s < (int)sizeof(send_buf); s+=2){
                 printf("%02hhX%02hhX ", send_buf[s], send_buf[s+1]);
            }
            
//======================================================================================



                printf("\n[UART][SEND] Write to addr 0x%04X, bytes=%i ... ", lineAddr, lineBytesNum);

                // Snapshot the 64 data bytes we are about to write, so we can
                // verify them after read-back. send_buf[4..67] holds the data
                // (send_buf[0..3] = len, cmd, addrH, addrL).
                char written_data[64] = {};
                for (int s = 0; s < 64; s++) {
                    written_data[s] = send_buf[s + 4];
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
                int mismatches = 0;
                for (int k = 0; k < 32; k++) {
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

           break;
    }


}


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


if (start_app == 1){
printf("[UART][SEND] Starting App ...\n");
memset (send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x02;    // Length of data in frame, include this byte also
send_buf[1] = START_APPLICATION;
UART_Send(serialPort_fd, send_buf, send_buf[0]);

}

       

return 1; //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<,

}


