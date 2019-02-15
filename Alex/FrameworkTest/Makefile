LDFLAGS=-pthread
CFLAGS = -I.
CC=clang
DEPS = algorithm.h

%.o: %.c $(DEPS)
	$(CC) $(LDFLAGS) -c -o $@ $< $(CFLAGS)

framework: framework.o wrapper.o algorithm.o global.o
	$(CC) $(LDFLAGS) -o framework framework.o wrapper.o algorithm.o global.o

clean:
	find . -name "*.o" -type f -delete
	rm framework
