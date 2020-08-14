#include <unistd.h>

#include "fw_converter.h"
#include "uart_procedures.h"

using namespace std;

#define MAX_BYTES_TO_SEND   68
#define MAX_BYTES_TO_RECV   68 
#define PREAM_FROM_DEVICE   0xAA

int main(int argc, char** argv) {
printf("Firmware uploader v1.1\n");

char serial_name[32] = {};
char serial_speed[6] = {};
char fw_path[64] = {};
int param_count = 0;
short verbose = 0;
int num_bytes = 0; 
uint8_t read_buf[MAX_BYTES_TO_RECV] = {};


	for (int i = 1; i < argc; ++i) {

        if (strcmp(argv[i], "-h") == 0) {
        		printf("Cli arguments keys: \n");
        		printf("     -D     Port name (e.g. /dev/ttyUSB0)\n");
        		printf("     -b     Port speed (e.g. 9600)\n");
        		printf("     -f     Path to firmware\n");      		        		
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
        		strcpy(fw_path, argv[i+1]);
        		++param_count;
        	} else {
        		printf("Define firmware path.\n");
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
int serial_port = UART_Init(serial_name, serial_speed);


printf("Firmware %s\n\n", fw_path);


printf("Trying to found device ... \n");
UART_Recv(serial_port, read_buf, sizeof(read_buf), num_bytes);

char msg[] = { '\x55', '\x02', '\x01'};
write(serial_port, msg, sizeof(msg));
usleep(300);

UART_Recv(serial_port, read_buf, sizeof(read_buf), num_bytes);

   if (num_bytes > 0) 
   {
            printf("pream def= %X", PREAM_FROM_DEVICE);
            if (read_buf[0] == PREAM_FROM_DEVICE) {
                printf("Device found!\n");
            }
            else {
                printf("ERROR Wrong responce code!\n");        
            } 
    } else 
    {
        printf("ERROR Device not found!\n");
    }

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


