#include "uart_procedures.h"
using namespace std;

int UART_Baud(int baud)
{
    switch (baud) {
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    default: 
        return 0;
    }
}

int UART_Init(char serial_name[32], char serial_speed[6]){

    int serial_port = open(serial_name, O_RDWR);

    // Check for errors
    if (serial_port < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    }

    // Create new termios struc, we call it 'tty' for convention
    struct termios tty;
    memset(&tty, 0, sizeof tty);

    // Read in existing settings, and handle any error
    if(tcgetattr(serial_port, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    //tty.c_cflag |= PARENB;  // Set parity bit, enabling parity
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    //tty.c_cflag |= CSTOPB;  // Set stop field, two stop bits used in communication
    //tty.c_cflag |= CS5; // 5 bits per byte
    //tty.c_cflag |= CS6; // 6 bits per byte
    //tty.c_cflag |= CS7; // 7 bits per byte
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    //tty.c_cflag |= CRTSCTS;  // Enable RTS/CTS hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON; //Disabling Canonical Mode
    tty.c_lflag &= ~ECHO; // Disable echo
    //tty.c_lflag &= ~ECHOE; // Disable erasure
    //tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT IN LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT IN LINUX)
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    //tty.c_iflag |= IXON ; // Turn on s/w flow ctrl
    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;
    // Set in/out baud rate to be 9600
    cfsetispeed(&tty, UART_Baud(atoi(serial_speed)));
    cfsetospeed(&tty, UART_Baud(atoi(serial_speed)));

    //printf("Speed in int = %i\n",cfgetispeed(&tty));
    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) 
        {
            printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        }

    return serial_port;

}

int UART_Recv(int serial_port, char *read_buf, int size_buf){
    memset(read_buf, '\0', size_buf);
    char ch;
    int ret = 0;
    int bytes_count = 0;
    while ((ret = read(serial_port, &ch, 1)) > 0)
        {
            if (bytes_count < size_buf){
                read_buf[bytes_count] = ch;
                //printf("Index %i  Char = %hhX \n", bytes_count, (unsigned)ch);
                bytes_count++;
            } else {
                break;
            }
        }
    if (read_buf[0] != (char)PREAM_FROM_DEVICE){
        bytes_count = -1;
    } 
    return bytes_count;
}


void UART_Send(int serial_port, char *data, int arr_size){
    char syn_code[1] = {PREAM_TO_DEVICE};
    write(serial_port, syn_code, 1);
    write(serial_port, data, arr_size);
    usleep(300);
    }
