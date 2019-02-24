LDFLAGS=-pthread
CFLAGS = -I.
CC=gcc
DEPS = algorithm.h

%.o: %.c $(DEPS)
	$(CC) $(LDFLAGS) -c -o $@ $< $(CFLAGS) -lm

framework: framework.o wrapper.o algorithm.o global.o
	$(CC) $(LDFLAGS) -o framework framework.o wrapper.o algorithm.o global.o -lm

clean:
	find . -name "*.o" -type f -delete
	rm framework