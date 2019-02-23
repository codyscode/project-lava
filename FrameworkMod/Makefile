LDFLAGS=-pthread
CFLAGS = -I.
CC=gcc
DEPS = algorithm.h

%.o: %.c $(DEPS)
	$(CC) $(LDFLAGS) -std=c99 -c -o $@ $< $(CFLAGS)

framework: framework.o wrapper.o algorithm.o global.o
	$(CC) $(LDFLAGS) -o framework framework.o wrapper.o algorithm.o global.o

clean:
	find . -name "*.o" -type f -delete
	rm framework
