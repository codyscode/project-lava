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

#Object files
OBJS = $(addprefix $(FWF), $(SRCS:.c=.o)) $(AP)*.o

#framework executable
TARGET = framework

.PHONY: clean help

ifneq ("$(wildcard $(AP))","")
all: clean $(TARGET)
else
all: 
	$(info Need to specify a path to your algorithm folder. 
	$(info Example: make AP=Algorithm1/)
	$(info For all arguments, run: make help)
	$(error )
endif

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): $(addprefix $(FWF), $(SRCS)) $(AP)*.c $(addprefix $(FWF), $(DEPS))
	$(MAKE) -C $(FWF)
	$(MAKE) -C $(AP)

ifeq ($(AP),)
clean:
	$(info To clean the algorithm folder, you need to specify a path to the)
	$(info makefile. Example: make AM=Algorithm1/)
	$(MAKE) clean -C $(FWF)
	find . -name $(TARGET) -type f -delete
else
clean:
	$(MAKE) clean -C $(FWF)
	find . -name $(TARGET) -type f -delete
	$(MAKE) clean -C $(AP)
endif

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
	$(info - (optional) -NM)
	$(info -            Example: -NM)
	$(info -            Description: Tells the high level makefile that you)
	$(info -            do not have a makefile for your algorithm If this is)
	$(info -            not included, make will look for a makefile in the)
	$(info -            folder you pass in)
	$(info -)
	$(info - make [options] clean)
	$(info -)
	$(info - Options:)
	$(info -)
	$(info - (optional) AP=relative/path/to/algorithm)
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
