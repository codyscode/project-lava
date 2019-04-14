#ifndef GLOBAL_H
#define GLOBAL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <signal.h>

//Constants for upper limit of queues
#define MAX_NUM_QUEUES 16
#define MAX_NUM_INPUT_QUEUES 8
#define MAX_NUM_OUTPUT_QUEUES 8

//Constants for threads
#define BUFFERSIZE 1024

//Defines how many ticks an algorithm should run for
#define RUNTIME 30

//Used to determine packet size
//8000 Allows much faster packet generation
#define MIN_PAYLOAD_SIZE 64
#define MAX_PAYLOAD_SIZE 8000

//Maximum packet size
#define MIN_PACKET_SIZE 192 + MIN_PAYLOAD_SIZE
#define MAX_PACKET_SIZE 192 + MAX_PAYLOAD_SIZE

//Number of unique flows that each input queue generates
#define FLOWS_PER_QUEUE 8

//Indicates whether a packet is there or not
#define NOT_OCCUPIED 0
#define OCCUPIED 1

//Minimum number of queues required
#define MIN_INPUT_QUEUE_COUNT 1
#define MIN_OUTPUT_QUEUE_COUNT 1

//Major indices in the buffer
#define FIRST_INDEX 0
#define LAST_INDEX BUFFERSIZE - 1

//Define a memory fence that tells the compiler to not reorder instructions
//In order to make sure writes are in order
#define FENCE() \
   __asm__ volatile ("mfence" ::: "memory");

#if defined (__linux__)
    #define SUPPORTED_PLATFORM 1
#else 
    #define SUPPORTED_PLATFORM 0
#endif

//function type for a method that returns a void * method with void * arguments
typedef void* (*function)(void * args);

//Used for reading the time stamp counter
typedef unsigned long long tsc_t;

//Data structure to represent a packet.
//length (size_t) - The total size of the data memeber for the packet
//flow (size_t) - The flow of the packet
//order (size_t) - The order of the packet within its flow
//data (unsigned char array) - Payload of the packet
typedef struct packet{ 
    size_t length;
    size_t flow;
    size_t order;
    unsigned char data[MAX_PAYLOAD_SIZE];
}packet_t;

//Data field for the queue
typedef struct data{
    size_t isOccupied;
    packet_t packet;
}data_t;

//Data structure for Queue:
//toRead (size_t) - The next unread position in the queue
//toWrite (size_t) - The next position to write to in the queue
//count (size_t) - The number of packets read by the queue
//data (packet_t array) - Space for packets in the queue
typedef struct Queue {
    size_t toRead;
    size_t toWrite;
    size_t count;
    data_t data[BUFFERSIZE];
}queue_t;

//Arguments to be passed to input threads
//queue (*queue_t) - pointer to the first queue for the thread to write to/process
//queueNum (size_t) - The queue number relative to other queues, used to index in queue array
//coreNum (size_t) - used to define which core the processing queue should be assigned to
typedef struct threadArgs{
    queue_t *queue;
    size_t queueNum;
    size_t coreNum;
}threadArgs_t;

//threadID (pthread_t) - The Id for the thread
//threadArgs (threadArgs_t) - Arguments to be passed to input threads
//queue (queue_t) - a queue struct
typedef struct io{
    pthread_t threadID;
    threadArgs_t threadArgs;
    queue_t queue;
}io_t;

//initialize array of input and ouput threads
io_t input[MAX_NUM_INPUT_QUEUES];
io_t output[MAX_NUM_OUTPUT_QUEUES];

//Used for number of input and output queues
size_t inputQueueCount;
size_t outputQueueCount;

// flag used to start moving packets - used by alarm functions
int startFlag;

// flag used to end algorithm - used by alarm functions
int endFlag; 

void set_thread_props(int tgt_core);
void assign_to_zero();
void sig_alrm(int signo);
void alarm_init();
void alarm_start();

char* getName();
function get_fill_method();
function get_drain_method();
void * run(void * args);

#endif
