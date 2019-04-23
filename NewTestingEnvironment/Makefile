#-I: 		Tell the compiler to look in the current directory
#-g: 		Addds debugging information
#-Wall: 	Turns on most compiler warnings
#-std=c99:	Tells which standard we want to use
CFLAGS = -I. -g -Wall -std=c99 #-O3

#The compiler: gcc for C program, define as g++ for C++
CC = gcc

#-lm: 			Math 
#-lpthread:		library and p
LIBS = -lm -lpthread

#Framework Folder
FWF = FrameworkSRC/

#C soure files
SRCS = framework.c wrapper.c global.c

#Object files
OBJS = $(addprefix $(FWF), $(SRCS:.c=.o)) $(APATH:.c=.o)

#framework executable
TARGET = framework

.PHONY: clean help

ifneq ("$(wildcard $(APATH))","")
all: $(TARGET)
else
all: 
	$(info Need to specify path to algorithm. Example: make APATH=Algorithm1/algorithm.c)
	$(info For all arguments, run: make help)
	$(error )
endif

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

ifeq ($(AMAKE),)
$(OBJS): $(addprefix $(FWF), $(SRCS))
	$(CC) $(CFLAGS) -c $(APATH) -o $(APATH:.c=.o) $(LIBS)
	$(MAKE) -C $(FWF)
else
$(OBJS): $(addprefix $(FWF), $(SRCS))
	$(MAKE) -C $(AMAKE)
	$(MAKE) -C $(FWF)
endif

ifeq ($(AMAKE),)
clean:
	$(info To clean the algorithm folder, you need to specify a path to the makefile. Example: make APATH=Algorithm1/algorithm.c)
	$(MAKE) clean -C $(FWF)
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
else
clean:
	$(MAKE) clean -C $(AMAKE)
	$(MAKE) clean -C $(FWF)
	find . -name "*.csv" -type f -delete
	find . -name $(TARGET) -type f -delete
endif

help:	
	$(info make [options])
	$(info )
	$(info Options:)
	$(info )
	$(info - (required) APATH=relative/path/to/algorithm)
	$(info -            Example: APATH=Algorithm1/algorithm.c)
	$(info -            Description: Path to the algorithm.c file which contains the proper code)
	$(info )
	$(info - (optional) AMAKE=relative/path/to/folder/with/makefile)
	$(info -            Example: AMAKE=Algorithm1/)
	$(info -            Description: Path to the folder that contains the makefile to compile the algorithm and any other files.) 
	$(info -            If this is not included, make will compile your algorithm.c file with the following code:)
	$(info -            $(CC) $(CFLAGS) -c algorithm.c -o algorithm.o $(LIBS))
	$(info )
	$(info make [options] clean)
	$(info )
	$(info Options:)
	$(info )
	$(info - (required) AMAKE=relative/path/to/algorithm)
	$(info -            Example: APATH=Algorithm1/algorithm.c)
	$(info )
	$(info Algorithm folder structure used in examples:)
	$(info Algorithm1/)
	$(info .    |_ algorithm.c)
	$(info .    |_ Makefile)
	$(info )
	exit
