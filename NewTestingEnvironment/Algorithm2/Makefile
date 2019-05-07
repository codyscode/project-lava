#-I: 		Tell the compiler to look in the current directory
#-g: 		Addds debugging information
#-Wall: 	Turns on most compiler warnings
#-std=c99:	Tells which standard we want to use
#-march=native: used for DPDK's memcpy
CFLAGS = -I. -g -Wall -std=c99 -march=native #-O3

#The compiler: gcc for C program, define as g++ for C++
CC = gcc

#-lm: 			Math 
#-lpthread:		library and p
LIBS = -lm -lpthread

#.c source files
SRC = algorithm.c

#File that should be created
TARGET = algorithm.o

.PHONY: clean

#Move the algorithm to the framework folder then call make
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $@ $(LIBS)

clean:
	find . -name "*.o" -type f -delete