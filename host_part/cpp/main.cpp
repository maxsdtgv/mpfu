#include <unistd.h>
#include "fw_converter.h"
#include "uart_procedures.h"

using namespace std;

#define MAX_BYTES_TO_SEND   68
#define MAX_BYTES_TO_RECV   67

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
bool device_found = false;

int serial_port = 0;
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


bool FoundDevice(){

printf("[UART][SEND] Trying to found device ... \n");

//======= Ping to device =====================================================================================
    memset (send_buf, 0, sizeof(send_buf));
    send_buf[0] = 0x04;    // Length of data in frame, include this byte also
    send_buf[1] = READ_FROM_MEM;
    send_buf[2] = 0x80;
    send_buf[3] = 0x06;

    UART_Send(serial_port, send_buf, send_buf[0]);
    received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);
    if (received_bytes == -1) {
        printf("     ERROR Device not found!\n");
        return false;
    } else {
            if (read_buf[1] == 0x04 && read_buf[2] == (char)READ_FROM_MEM){
                printf("             Device ID: %hhX.%hhX\n", read_buf[3], read_buf[4]);
                send_buf[3] = 0x05;
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);
                printf("             Device revision: %hhX.%hhX\n", read_buf[3], read_buf[4]);
                return true;
            }else{
                printf("wwww! %hhX  %hhX\n", read_buf[1], read_buf[2]);
                printf("Device respose with wrong code!\n");
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
serial_port = UART_Init(serial_name, serial_speed);
UART_Recv(serial_port, read_buf, sizeof(read_buf)/sizeof(*read_buf));   // Clear UART before send

device_found = FoundDevice();


if (!device_found){
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

    memset (tmp_buffer, 0, sizeof(tmp_buffer));
    strncpy(tmp_buffer, hex_buffer + 4, 4); //char * strncpy( char * destptr, const char * srcptr, size_t num );
    lineAddr = stoi (tmp_buffer, nullptr, 16); //int stoi( const std::string& str, std::size_t* pos = 0, int base = 10 );
//    printf("%d \n", lineAddr);

    memset (tmp_buffer, 0, sizeof(tmp_buffer));
    strncpy(tmp_buffer, hex_buffer, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
    lineBytesNum = stoi (tmp_buffer, nullptr, 16); //int stoi( const std::string& str, std::size_t* pos = 0, int base = 10 );


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
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);
                    if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                        printf("[UART][READ] ERROR Wrong number of bytes was received!\n");
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
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                    if ((received_bytes == -1)) {
                        printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                        exit(6);
                    }
                if ((read_buf[2] != (char)SUCCESS_CODE)) {
                    printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
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
            UART_Send(serial_port, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                    printf("[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                }
            //printf("Read buf %s \n", read_buf); 
            memset (send_buf, 0, sizeof(send_buf));
            printf("[FW] Preparing string with modified App Reset Vector (ARV).\n");

            send_buf[0] = 0x44;    // Length of data in frame, include this byte also
            send_buf[1] = WRITE_TO_MEM;            
            send_buf[2] = 0x3F; // Read BL flags block
            send_buf[3] = 0xE0;
            for (int u = 0; u < MAX_BYTES_TO_SEND - 8; u++){
                send_buf[u+4] = read_buf[u+3];
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
            UART_Send(serial_port, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1)) {
                    printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                }
            if ((read_buf[2] != (char)SUCCESS_CODE)) {
                printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
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
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                    if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                        printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
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
                    send_buf[u + 4] = read_buf[u + 3];
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
printf("\n============= Data to 0x%04X, bytes=%i ... ", lineAddr, lineBytesNum);
            for (int s=0; s < (int)sizeof(send_buf); s+=2){
                 printf("%02hhX%02hhX ", send_buf[s], send_buf[s+1]);
            }
            
//======================================================================================



                printf("\n[UART][SEND] Write to addr 0x%04X, bytes=%i ... ", lineAddr, lineBytesNum);
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1)) {
                    printf("\n[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                    }
                printf(" SUCCESS.\n");           




//======================================================================================
printf("\n============= Read from 0x%04X ...              ", lineAddr);


            send_buf[0] = 0x04;    // Length of data in frame, include this byte also
            send_buf[1] = READ_FROM_MEM;
            UART_Send(serial_port, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                    printf("[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                }
 
            for (int s=1; s < (int)sizeof(read_buf); s+=2){
                 printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            }
            printf("\n SUCCESS.\n");
//=======================================================================================

           break;
    }


}


printf("\n============= Read from 0x0000\n");


            send_buf[0] = 0x04;    // Length of data in frame, include this byte also
            send_buf[1] = READ_FROM_MEM;
            send_buf[2] = 0x00; // Read BL flags block
            send_buf[3] = 0x00;
            UART_Send(serial_port, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                    printf("[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                }
 
            for (int s=3; s < (int)sizeof(read_buf); s+=2){
                 printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            }
            printf("\n SUCCESS.\n");

printf("\n============= Read from 0x3FE0\n");


            send_buf[0] = 0x04;    // Length of data in frame, include this byte also
            send_buf[1] = READ_FROM_MEM;
            send_buf[2] = 0x3F; // Read BL flags block
            send_buf[3] = 0xE0;
            UART_Send(serial_port, send_buf, send_buf[0]);
            received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);            
                if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                    printf("[UART][RECEIVE] ERROR Wrong number of bytes was received!\n");
                    exit(6);
                }
 
            for (int s=3; s < (int)sizeof(read_buf); s+=2){
                 printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            }
            printf("\n SUCCESS.\n");
}




if (read_app == 1){

printf("[FW] Saved firmware %s\n\n", saveFwFilename);
realpath(saveFwFilename, saveFwFilenameAbsolutePath);
saveFwFilenameFd.open(saveFwFilenameAbsolutePath);

printf("[UART][SEND] Reading MCU memory ...\n");

char temp2[7] = {};
memset (temp2, 0, sizeof(temp2));


memset (send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x04;    // Length of data in frame, include this byte also
send_buf[1] = READ_FROM_MEM;

for (int p = 0x0; p < 0x3FFF; p += 0x20){
            
            printf("[UART][READ] Reading addr > %02hhX%02hhX\n", ((p & 0xFF00)>>8), (p & 0x00FF));

                send_buf[2] = ((p & 0xFF00)>>8);
                send_buf[3] = (p & 0x00FF);
                UART_Send(serial_port, send_buf, send_buf[0]);
                received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);
                    if ((received_bytes == -1) | (received_bytes < MAX_BYTES_TO_RECV)) {
                        printf("\n[UART][READ] ERROR Wrong number of bytes was received! (%i)\n" , received_bytes);
                        exit(6);
                    }

            //for (int s=3; s < (int)sizeof(read_buf); s+=2){
            //     printf("%02hhX%02hhX ", read_buf[s], read_buf[s+1]);
            //}

            sprintf(temp2, "\n%02X%02X ", ((p & 0xFF00)>>8), (p & 0x00FF));
            saveFwFilenameFd.write(temp2, 6);

            for (int s=3; s < (int)sizeof(read_buf); s++){

                sprintf(temp2, "%02hhX", read_buf[s]);
                saveFwFilenameFd.write(temp2, 2);

            }


            saveFwFilenameFd.flush();



}


}


if (start_app == 1){
printf("[FW] Flasing done.\n");
printf("[UART][SEND] Starting App ...\n");
memset (send_buf, 0, sizeof(send_buf));
send_buf[0] = 0x02;    // Length of data in frame, include this byte also
send_buf[1] = START_APPLICATION;
UART_Send(serial_port, send_buf, send_buf[0]);
received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);     

}

       

return 1; //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<,

}


