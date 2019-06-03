LDFLAGS=-pthread
CFLAGS = -I.
CC=gcc

%.o: %.c
	$(CC) $(LDFLAGS) -std=c99 -c -o $@ $< $(CFLAGS)

framework: framework.o wrapper.o  global.o
	$(CC) $(LDFLAGS) -g -o framework framework.o wrapper.o global.o -lm

clean:
	find . -name "*.o" -type f -delete
	find . -name "*.csv" -type f -delete
	rm framework
