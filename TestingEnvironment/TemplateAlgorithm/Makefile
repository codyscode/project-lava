LDFLAGS=-pthread
CFLAGS = -I.
CC=gcc

subsystem: algorithm.o
	mv algorithm.o ../Framework/algorithm.o
	cd ../Framework && $(MAKE)

algorithm.o: algorithm.c
	$(CC) $(LDFLAGS) -c -std=c99 algorithm.c
