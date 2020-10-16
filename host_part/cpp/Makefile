CC=g++
CFLAGS=-c -Wall -lstdc

all: mpfu

mpfu: main.o fw_converter.o uart_procedures.o
	$(CC) main.o fw_converter.o uart_procedures.o -o mpfu

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

fw_converter.o: fw_converter.cpp
	$(CC) $(CFLAGS) fw_converter.cpp

uart_procedures.o: uart_procedures.cpp
	$(CC) $(CFLAGS) uart_procedures.cpp

clean:
	rm -rf *.o mpfu