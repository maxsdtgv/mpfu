// C library headers
#include <stdio.h>
#include <string.h>
#include <string>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

int UART_Baud(int);
int UART_Init(char[32], char[6]);
void UART_Recv(int, uint8_t*, size_t, int&);