// C library headers
#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

#define PREAM_TO_DEVICE     0x55
#define PREAM_FROM_DEVICE   0xAA

//=============    SUPPORTED CODES IN RETURNED COMMANDS ================
#define ERROR_CODE			0xFF
#define SUCCESS_CODE		0xEE
//#define PING_REQUEST		0x01
#define READ_FROM_MEM		0x02
#define WRITE_TO_MEM		0x04
#define START_APPLICATION	0x0F
//====================================================


int UART_Baud(int);
int UART_Init(char[32], char[6]);
int UART_Recv(int, char*, int);
void UART_Send(int, char*, int);
void ProgressBar(char*, int, int, int);
void UART_Clear(int);
