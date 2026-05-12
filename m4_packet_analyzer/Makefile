CC=gcc
CFLAGS=-Wall -Wextra -g
LIBS=-lpcap

TARGET=packet_analyzer
SRC=packet_analyzer.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)