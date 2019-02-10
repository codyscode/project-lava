LDFLAGS=-pthread
CC=gcc

initial-queuecode: queuecode.o
	$(CC) $(LDFLAGS) -o queuecode queuecode.o

clean:
	rm -f queuecode.o