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
    case 115200:
        return B115200;
    default: 
        return 0;
    }
}


char speed[6] = {};

int UART_Init(char serial_name[32], char serial_speed[6]){

    strcpy(speed, serial_speed);

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
    //tty.c_oflag |= OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)

    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    //tty.c_oflag |= ONLCR; // Prevent conversion of newline to carriage return/line feed


    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT IN LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT IN LINUX)
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IEXTEN); // Disable any special handling of received bytes
    //tty.c_iflag |= (IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    //tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    //tty.c_iflag |= IXON ; // Turn on s/w flow ctrl
        tty.c_cc[VTIME]    = 0;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
        tty.c_cc[VMIN]     = 0;     /* blocking read until 1 character arrives */

        tty.c_cc[VINTR]    = 0;     /* Ctrl-c, ETX, 0x03*/ 
        tty.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
        tty.c_cc[VERASE]   = 0;     /* del */
        tty.c_cc[VKILL]    = 0;     /* @ */
        tty.c_cc[VEOF]     = 0;     /* Ctrl-d */
        tty.c_cc[VSWTC]    = 0;     /* '\0' */
        tty.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
        tty.c_cc[VSTOP]    = 0;     /* Ctrl-s */
        tty.c_cc[VSUSP]    = 0;     /* Ctrl-z */
        tty.c_cc[VEOL]     = 0;     /* '\0' */
        tty.c_cc[VREPRINT] = 0;     /* Ctrl-r */
        tty.c_cc[VDISCARD] = 0;     /* Ctrl-u */
        tty.c_cc[VWERASE]  = 0;     /* Ctrl-w */
        tty.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
        tty.c_cc[VEOL2]    = 0;     /* '\0' */
    // Set in/out baud rate to be 9600
    cfsetispeed(&tty, UART_Baud(atoi(speed)));
    cfsetospeed(&tty, UART_Baud(atoi(speed)));

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
    bool start_frame_found = false;
    int bytes_count = 0;
    int timeout = 0;
    int delay_read = 0;

    delay_read = 1000000/(atoi(speed)/10);

    do {
            usleep(delay_read*10);
            timeout++;
            read(serial_port, &ch, 1);


            if (start_frame_found == true){
                read_buf[bytes_count] = ch;
                //printf("Index %i  Char = %hhX \n", bytes_count, ch);
                bytes_count++;
                if (bytes_count == 1){
                    size_buf = (int)(ch);
                }
            }

             if ((bytes_count == 0) && (ch == (char)PREAM_FROM_DEVICE)){
                start_frame_found = true;
            }           

            if (timeout == delay_read*size_buf) {break;}



    } while (bytes_count < size_buf);

    if (start_frame_found != true){
        bytes_count = -1;
    } 


//    do {
//            usleep(100);
//            ret = read(serial_port, &ch, 1);
//
//            if (bytes_count < size_buf){
//                read_buf[bytes_count] = ch;
//                //printf("Index %i  Char = %hhX \n", bytes_count, ch);
//                bytes_count++;
//            } else {
//                break;
//            }
//    } while (ret > 0);
//
//    if (read_buf[0] != (char)PREAM_FROM_DEVICE){
//        bytes_count = -1;
//    } 

    return bytes_count;
}


void UART_Send(int serial_port, char *data, int arr_size){
    char syn_code[1] = {PREAM_TO_DEVICE};
    write(serial_port, syn_code, 1);
    usleep(500);
    write(serial_port, data, arr_size);
    usleep(10000);
    }

