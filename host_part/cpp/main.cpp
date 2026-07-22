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
#define RESET_VECTOR_WORDS  4      // words 0x0000-0x0003 reserved for GOTO bootloader
#define RESET_VECTOR_BYTES  8      // 4 words * 2 bytes


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
        case 0:
                printf("[FW] String with addr=0x0000 found.\n");            
            if (lineBytesNum > 12){
                printf("[FW] Will write data with offset 0x0004 Data %s \n", hex_buffer+24);
                printf("[UART][READ] Read flags block from device.\n");
                send_buf[0] = 0x04;    // Length of data in frame, include this byte also
                send_buf[1] = READ_FROM_MEM;
                send_buf[2] = 0x00; // Try to read from 0x0000
                send_buf[3] = 0x00; 
                UART_Send(serialPort_fd, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);
                    if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                        printf("[UART][READ] ERROR Wrong number of bytes or corrupted frame was received!\n");
                        exit(6);
                    }
                //printf("Read buf %s \n", read_buf);
                memset (send_buf, 0, sizeof(send_buf));
                printf("[FW] Preparing string with data.\n");
                send_buf[0] = 0x44;    // Length of data in frame, include this byte also
                send_buf[1] = WRITE_TO_MEM;            
                send_buf[2] = 0x00; // Read BL flags block
                send_buf[3] = 0x00;
                for (int u = 0; u < MAX_BYTES_TO_SEND - 4; u++){
                    send_buf[u+4] = read_buf[u+3];
                }
                tmp = 0;
                for (int u = 12; u < lineBytesNum; u++){
                    memset (tmp_buffer, 0, sizeof(tmp_buffer));
                    strncpy(tmp_buffer, hex_buffer + 24 + tmp, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                    send_buf[u] = stoi (tmp_buffer, nullptr, 16);
                    tmp += 2;
                }      
                printf("[UART][SEND] Write block with modified data, addr=0x%hhX%hhX ... ", send_buf[2], send_buf[3]);
                UART_Send(serialPort_fd, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);            
                    if ((received_bytes == -1)) {
                        printf("\n[UART][RECEIVE] ERROR Wrong number of bytes or corrupted frame was received!\n");
                        exit(6);
                    }
                if ((read_buf[1] != (char)SUCCESS_CODE)) {
                    printf("\n[UART][RECEIVE] ERROR Device reported unsuccessful code!\n");
                    exit(6);
                }        
                printf(" SUCCESS.\n");

for (int s=0; s < (int)sizeof(send_buf); s+=2){
    printf("%i Send buf %02hhX%02hhX \n", s/2, send_buf[s], send_buf[s+1]);
}

            }



            printf("[FW] String with Main Reset Vector (MRV) found. Data %s \n", hex_buffer+8);
            printf("[UART][RECEIVE] Read flags block from device.\n");
            send_buf[0] = 0x04;    // Length of data in frame, include this byte also
            send_buf[1] = READ_FROM_MEM;
            send_buf[2] = 0x3F; // Read BL flags block
            send_buf[3] = 0xE0;
            UART_Send(serialPort_fd, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                    printf("[UART][RECEIVE] ERROR Wrong number of bytes or corrupted frame was received!\n");
                    exit(6);
                }
            //printf("Read buf %s \n", read_buf); 
            memset (send_buf, 0, sizeof(send_buf));
            printf("[FW] Preparing flags with modified App Reset Vector (ARV).\n");

            send_buf[0] = 0x44;    // Length of data in frame, include this byte also
            send_buf[1] = WRITE_TO_MEM;            
            send_buf[2] = 0x3F; // Read BL flags block
            send_buf[3] = 0xE0;
            for (int u = 0; u < MAX_BYTES_TO_SEND - 8; u++){
                send_buf[u+4] = read_buf[u+2];
            }
            tmp = 0;
            for (int u = MAX_BYTES_TO_SEND - 8; u < MAX_BYTES_TO_SEND-4; u++){
                memset (tmp_buffer, 0, sizeof(tmp_buffer));
                strncpy(tmp_buffer, hex_buffer + 16 + tmp, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                send_buf[u] = stoi (tmp_buffer, nullptr, 16);
                tmp += 2;
            }         
            send_buf[64] = 0x3F; // Read BL flags block
            send_buf[65] = 0xFF;
            send_buf[66] = 0x3F; // Read BL flags block
            send_buf[67] = 0xFF;     
                     
            printf("[UART][SEND] Write flags block with ARV, addr=0x%hhX%hhX ... ", send_buf[2], send_buf[3]);
            UART_Send(serialPort_fd, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1)) {
                    printf("\n[UART][RECEIVE] ERROR Wrong number of bytes or corrupted frame was received!\n");
                    exit(6);
                }
            if ((read_buf[1] != (char)SUCCESS_CODE)) {
                printf("\n[UART][RECEIVE] ERROR Device reported unsuccessful code!\n");
                exit(6);
            }        
            printf(" SUCCESS.\n");
//for (int s=0; s < (int)sizeof(send_buf); s+=2){
//    printf("%i Send buf %02hhX%02hhX \n", s/2, send_buf[s], send_buf[s+1]);
//}
            break;
        default:
            if (lineBytesNum != 0x44){
                printf("[FW] Block is not full\n");

                memset (send_buf, 0, sizeof(send_buf));

                send_buf[0] = 0x04;    // Length of data in frame, include this byte also
                send_buf[1] = READ_FROM_MEM;

                memset (tmp_buffer, 0, sizeof(tmp_buffer));
                strncpy(tmp_buffer, hex_buffer + 4, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                send_buf[2] = stoi (tmp_buffer, nullptr, 16);

                memset (tmp_buffer, 0, sizeof(tmp_buffer));
                strncpy(tmp_buffer, hex_buffer + 6, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                send_buf[3] = stoi (tmp_buffer, nullptr, 16);

                printf("[UART][RECEIVE] Read block from addr=0x%02hhX%02hhX...", send_buf[2], send_buf[3]);
                UART_Send(serialPort_fd, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serialPort_fd, read_buf, MAX_BYTES_TO_RECV);            
                    if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                        printf("\n[UART][RECEIVE] ERROR Wrong number of bytes or corrupted frame was received!\n");
                        exit(6);
                        }
                printf(" SUCCESS.\n");

            //for (int s=0; s < (int)sizeof(read_buf)+1; s+=2){
            //    printf("%i Read buf %02hhX%02hhX \n", s/2, read_buf[s], read_buf[s+1]);
            //}
                printf("[FW] Preparing full line...\n");
                send_buf[0] = 0x44;    // Length of data in frame, include this byte also
                send_buf[1] = WRITE_TO_MEM;

                tmp = 0;
                for (int u = 0; u < lineBytesNum-4; u++){
                    memset (tmp_buffer, 0, sizeof(tmp_buffer));
                    strncpy(tmp_buffer, hex_buffer + 8 + tmp, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                    send_buf[u + 4] = stoi (tmp_buffer, NULL, 16);
                    tmp += 2;
                    }

                for (int u = lineBytesNum - 4; u < MAX_BYTES_TO_SEND; u++){
                    send_buf[u + 4] = read_buf[u + 2];
                    } 



            //for (int s=0; s < (int)sizeof(send_buf); s+=2){
            //    printf("%i Send buf %02hhX%02hhX \n", s/2, send_buf[s], send_buf[s+1]);
            //}


            } else {
                tmp = 0;
                for (int u = 0; u < MAX_BYTES_TO_SEND; u++){
                    memset (tmp_buffer, 0, sizeof(tmp_buffer));
                    strncpy(tmp_buffer, hex_buffer + tmp, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
                    send_buf[u] = stoi (tmp_buffer, nullptr, 16);
                    tmp += 2;
                    }
            }  


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


