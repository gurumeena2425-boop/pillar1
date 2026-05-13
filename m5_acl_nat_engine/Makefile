CC=gcc
CFLAGS=-Wall -Wextra -g

TARGET=acl_nat_engine
SRC=acl_nat_engine.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f output/decision_log.txt