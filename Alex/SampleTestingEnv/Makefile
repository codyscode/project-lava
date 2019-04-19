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

#Framework Folder
FWF = Framework/

#Algorithm Folder
ALGF = Algorithm1/

#Algorithm.c file. Default is algorithm1
ALGPATH = $(ALGF)algorithm.c

#C soure files
SRCS = framework.c wrapper.c global.c

#Object files
OBJS = $(addprefix $(FWF), $(SRCS:.c=.o)) $(ALGPATH:.c=.o)

#framework executable
TARGET = framework

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS):
ifdef $(ALGMAKE)
	$(MAKE) -C $(ALGF)
else
	$(CC) $(CFLAGS) -c $(ALGPATH) -o $(ALGPATH:.c=.o) $(LIBS)
endif
	$(MAKE) -C $(FWF)

clean:
ifdef $(CLEANPATH)
	$(MAKE) clean -C $(CLEANPATH)
endif
	$(MAKE) clean -C $(FWF)
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
