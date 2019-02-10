LDFLAGS=-pthread
CFLAGS = -I.
CC=gcc
DEPS = algorithm.h

%.o: %.c $(DEPS)
	$(CC) $(LDFLAGS) -c -o $@ $< $(CFLAGS)

framework: framework.o algorithm.o
	$(CC) $(LDFLAGS) -o framework framework.o algorithm.o

clean:
	rm -f algorithm.o framework.o