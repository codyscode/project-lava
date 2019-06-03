#-I: 		Tell the compiler to look in the current directory
#-g: 		Addds debugging information
#-Wall: 	Turns on most compiler warnings
#-std=c99:	Tells which standard we want to use
CFLAGS = -I. -g -Wall -std=c99 #-O3

#The compiler: gcc for C program, define as g++ for C++
CC = gcc

#header file dependencies
DEPS = global.h wrapper.h

#-lm: 			Math 
#-lpthread:		library and p
LIBS = -lm -lpthread

#Framework Folder
FWF = FrameworkSRC/

#C soure files
SRCS = framework.c wrapper.c global.c

#Default path
AP=Algorithm/

#Object files
OBJS = $(addprefix $(FWF), $(SRCS:.c=.o)) $(AP)*.o

#framework executable
TARGET = framework

.PHONY: clean help

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): $(addprefix $(FWF), $(SRCS)) $(AP)*.c $(addprefix $(FWF), $(DEPS))
	$(MAKE) -C $(FWF)
	$(MAKE) -C $(AP)

clean:
	$(MAKE) clean -C $(FWF)
	find . -name $(TARGET) -type f -delete
	$(MAKE) clean -C $(AP)
	find . -name "*.csv" -type f -delete

help:	
	$(info - make [options])
	$(info -)
	$(info - Options:)
	$(info -)
	$(info - (required) AP=relative/path/to/algorithm/folder)
	$(info -            Example: AP=Algorithm1/)
	$(info -            Description: Path to the algorithm folder which)
	$(info -            contains the .c file and makefile)
	$(info -)
	$(info - make [options] clean)
	$(info -)
	$(info - Options:)
	$(info -)
	$(info - (optional) AP=relative/path/to/algorithm/folder)
	$(info -            Example: AP=Algorithm1/)
	$(info -            Description: Will call the clean target in your)
	$(info -            makefile. If this is not included then the .o file)
	$(info -            will not be removed)
	$(info -)
	$(info - Algorithm folder structure used in examples:)
	$(info - Algorithm1/)
	$(info -    |_ algorithm.c)
	$(info -    |_ Makefile)
	$(info -)
	exit
