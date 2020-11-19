// C library headers
#include <stdio.h>
#include <string.h>
#include <string>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

#define PREAM_TO_DEVICE     "55"
#define PREAM_FROM_DEVICE   "AA"

//=============    SUPPORTED CODES IN RETURNED COMMANDS ================
#define ERROR_CODE			"FF"
#define SUCCESS_CODE		"EE"
//#define PING_REQUEST		"01"
#define READ_FROM_MEM		"02"
#define WRITE_TO_MEM		"04"
#define START_APPLICATION	"0F"
//====================================================


int UART_Baud(int);
int UART_Init(char[32], char[6]);
int UART_Recv(int, char*, int);
void UART_Send(int, char*, int);