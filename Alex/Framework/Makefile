#-I: 		Tell the compiler to look in the current directory
#-g: 		Addds debugging information
#-Wall: 	Turns on most compiler warnings
#-std=c99:	Tells which standard we want to use
CFLAGS = -I. -g -Wall -std=c99 #-O3

#The compiler: gcc for C program, define as g++ for C++
CC = gcc

#header file dependencies
DEPS = algorithm.h global.h wrapper.h

#-lm: 			Math 
#-lpthread:		library and p
LIBS = -lm -lpthread

#Algorithm.c file
ALG = algorithm

#C soure files
SRCS = framework.c wrapper.c global.c

#Object files
OBJS = $(SRCS:.c=.o) $(ALG).o

#framework executable
TARGET = framework

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

clean:
	find . -name "*.o" -type f -delete
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
