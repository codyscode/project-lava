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
OBJS = $(addprefix $(FWF), $(SRCS:.c=.o)) $(AP:.c=.o)

#framework executable
TARGET = framework

.PHONY: clean help

ifneq ("$(wildcard $(AP))","")
all: $(TARGET)
else
all: 
	$(info Need to specify path to algorithm. Example: make AP=Algorithm1/algorithm.c)
	$(info For all arguments, run: make help)
	$(error )
endif

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

ifeq ($(AM),)
$(OBJS): $(addprefix $(FWF), $(SRCS)) $(AP) $(addprefix $(FWF), $(DEPS))
	$(CC) $(CFLAGS) -c $(AP) -o $(AP:.c=.o) $(LIBS)
	$(MAKE) -C $(FWF)
else
$(OBJS): $(addprefix $(FWF), $(SRCS)) $(AP) $(addprefix $(FWF), $(DEPS))
	$(MAKE) -C $(FWF)
	$(MAKE) -C $(AM)
endif

ifeq ($(AM),)
clean:
	$(info To clean the algorithm folder, you need to specify a path to the makefile. Example: make AM=Algorithm1/)
	$(MAKE) clean -C $(FWF)
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
else
clean:
	$(MAKE) clean -C $(FWF)
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
	$(MAKE) clean -C $(AM)
endif

help:	
	$(info - make [options])
	$(info -)
	$(info - Options:)
	$(info -)
	$(info - (required) AP=relative/path/to/algorithm)
	$(info -            Example: AP=Algorithm1/algorithm.c)
	$(info -            Description: Path to the algorithm.c file which contains the proper code)
	$(info -)
	$(info - (optional) AM=relative/path/to/folder/with/makefile)
	$(info -            Example: AM=Algorithm1/)
	$(info -            Description: Path to the folder that contains the makefile to compile the algorithm and any other files.) 
	$(info -            If this is not included, make will compile your algorithm.c file with the following code:)
	$(info -            $(CC) $(CFLAGS) -c algorithm.c -o algorithm.o $(LIBS))
	$(info -)
	$(info - make [options] clean)
	$(info -)
	$(info - Options:)
	$(info -)
	$(info - (required) AM=relative/path/to/algorithm)
	$(info -            Example: AP=Algorithm1/algorithm.c)
	$(info -)
	$(info - Algorithm folder structure used in examples:)
	$(info - Algorithm1/)
	$(info -    |_ algorithm.c)
	$(info -    |_ Makefile)
	$(info -)
	exit
