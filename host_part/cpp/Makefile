CC=g++
CFLAGS=-c -Wall -std=c++11

all: mpfu

mpfu: main.o fw_converter.o uart_procedures.o deviceconfig.o
	$(CC) main.o fw_converter.o uart_procedures.o deviceconfig.o -o mpfu

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

fw_converter.o: fw_converter.cpp
	$(CC) $(CFLAGS) fw_converter.cpp

uart_procedures.o: uart_procedures.cpp
	$(CC) $(CFLAGS) uart_procedures.cpp

deviceconfig.o: deviceconfig.cpp
	$(CC) $(CFLAGS) deviceconfig.cpp

clean:
	rm -rf *.o mpfu
