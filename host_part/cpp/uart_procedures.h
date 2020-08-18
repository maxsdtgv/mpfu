// C library headers
#include <stdio.h>
#include <string.h>
#include <string>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

#define PREAM_TO_DEVICE     '\x55'
#define PREAM_FROM_DEVICE   '\xAA'

//=============    SUPPORTED CODES IN RETURNED COMMANDS ================
#define ERROR_CODE			'\xFF'
#define SUCCESS_CODE		'\xEE'
//#define PING_REQUEST		'\x01'
#define READ_FROM_MEM		'\x02'	
#define START_APPLICATION	'\x0F'
//====================================================


int UART_Baud(int);
int UART_Init(char[32], char[6]);
int UART_Recv(int, char*, int);
void UART_Send(int, char*, int);