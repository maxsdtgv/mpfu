#include <unistd.h>
#include "fw_converter.h"
#include "uart_procedures.h"

using namespace std;

#define MAX_BYTES_TO_SEND   66
#define MAX_BYTES_TO_RECV   66

int main(int argc, char** argv) {
printf("Microchip firmware uploader v1.1\n");

char serial_name[32] = {};
char serial_speed[6] = {};
char inFilename[64] = {};
char outFilename[64] = {};
int param_count = 0;
short verbose = 0;
int received_bytes = 0; 
char read_buf[MAX_BYTES_TO_RECV] = {};
char send_buf[MAX_BYTES_TO_SEND] = {};

                strcpy(outFilename, "./last_fw.cof");


	for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
        		printf("Cli arguments keys: \n");
        		printf("     -D     Port name (e.g. /dev/ttyUSB0)\n");
        		printf("     -b     Port speed (e.g. 9600)\n");
        		printf("     -f     Path to HEX file with firmware\n");      		        		
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
    }


if (param_count < 3) {
	printf("\nERROR Not enough parameters to continue.\n");
	return 1;
}

printf("Connect to %s, %s\n", serial_name, serial_speed);
printf("Firmware %s\n\n", inFilename);


fwConvertPic16F1xxx(inFilename, outFilename);


return 1; //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<,

int serial_port = UART_Init(serial_name, serial_speed);
UART_Recv(serial_port, read_buf, sizeof(read_buf)/sizeof(*read_buf));   // Clear UART before send



printf("Trying to found device ... \n");

//======= Ping to device =====================================================================================

    send_buf[0] = 0x04;    // Length of data in frame, include this byte also
    send_buf[1] = READ_FROM_MEM;
    send_buf[2] = 0x80;
    send_buf[3] = 0x00;

    UART_Send(serial_port, send_buf, send_buf[0]);

    received_bytes = UART_Recv(serial_port, read_buf, MAX_BYTES_TO_RECV);
    if (received_bytes == -1) {
        printf("     ERROR Device not found!\n");
        exit(6);
    } else {
            if (read_buf[1] == 0x04 && read_buf[2] == READ_FROM_MEM){
                printf("     Device found!\n");

                printf("     BL version: %hhX.%hhX\n",(unsigned)read_buf[3],(unsigned)read_buf[4]);
            }
            }



//============================================================================================


//============================================================================================
//while(1)
//    {
//
//
//        unsigned char msg[] = { '\x55', '\x04', '\x01', '\x31', '\x32'};
//        write(serial_port, msg, sizeof(msg));
//
//
//        // Allocate memory for read buffer, set size according to your needs
//        char read_buf [256];
//        memset(&read_buf, '\0', sizeof(read_buf));
//
//        // Read bytes. The behaviour of read() (e.g. does it block?,
//        // how long does it block for?) depends on the configuration
//        // settings above, specifically VMIN and VTIME
//        int num_bytes = read(serial_port, &read_buf, sizeof(read_buf));
//
//        // n is the number of bytes read. n may be 0 if no bytes were received, and can also be -1 to signal an error.
//        if (num_bytes < 0) 
//            {
//                printf("Error reading: %s", strerror(errno));
//            }
//
//        // Here we assume we received ASCII data, but you might be sending raw bytes (in that case, don't try and
//        // print it to the screen like this!)
//        printf("Read %i bytes. Received message: %s\n", num_bytes, read_buf);
//
//    }

}


