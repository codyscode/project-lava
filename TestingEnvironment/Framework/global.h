#ifndef GLOBAL_H
#define GLOBAL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>

//Constants for upper limit of queues
#define MAX_NUM_QUEUES 16
#define MAX_NUM_INPUT_QUEUES 8
#define MAX_NUM_OUTPUT_QUEUES 8

//Constants for threads
#define BUFFERSIZE 1024

//Defines how many seconds an algorithm should run for
#define RUNTIME 30

//Used to determine packet size
#define MAX_PAYLOAD_SIZE 9000
#define MIN_PAYLOAD_SIZE 64

#define MIN_PACKET_SIZE 192 + 64
#define MAX_PACKET_SIZE 192 + 9000

//Number of unique flows that each input queue generates
#define FLOWS_PER_QUEUE 8

#define FREE_SPACE_TO_WRITE 0

#define MIN_INPUT_QUEUE_COUNT 1
#define MIN_OUTPUT_QUEUE_COUNT 1

#define FIRST_INDEX 0
#define LAST_INDEX BUFFERSIZE - 1

//Define a memory fence that tells the compiler to not reorder instructions
//In order to make sure writes are in order
#define FENCE() \
   __asm__ volatile ("mfence" ::: "memory");

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

//Data structure for Queue:
//toRead (size_t) - The next unread position in the queue
//toWrite (size_t) - The next position to write to in the queue
//count (size_t) - The number of packets read by the queue
//data (packet_t array) - Space for packets in the queue
typedef struct Queue {
    size_t toRead;
    size_t toWrite;
    size_t count;
    packet_t data[BUFFERSIZE];
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

//input and output struct are mostly identical right now and mainly used to
//abstract the inputs resources from the outputs resources but we may want
//to add unique characteristics to each at some point.

//Initialize items for input threads
//queueCount (size_t) - The number of input queues
//threadIds (pthread_t) - The Id's for ech input thread
//threadArgs (threadArgs_t) - Arguments to be passed to input threads
//locks (pthread_mutex_t) - locks for each input queue
//queues (queue_t) - an array input queue structs
//finished (size_t) - flag that indicates when a given thread has generated x number of packets
typedef struct input{
    size_t queueCount;
    pthread_t threadIDs[MAX_NUM_INPUT_QUEUES]; 
    threadArgs_t threadArgs[MAX_NUM_INPUT_QUEUES];
    pthread_mutex_t locks[MAX_NUM_INPUT_QUEUES];
    queue_t queues[MAX_NUM_INPUT_QUEUES];
    size_t finished[MAX_NUM_INPUT_QUEUES];
}input_t;
input_t input;

//Initialize items for output threads
//queueCount (size_t) - The number of output queues
//threadIds (pthread_t) - The Id's for ech output thread
//threadArgs (threadArgs_t) - Arguments to be passed to output threads
//locks (pthread_mutex_t) - locks for each output queue
//queues (queue_t) - an array of output queue structs
//finished (size_t) - flag that indicates when a given thread has processed x number of packets
typedef struct output{
    size_t queueCount;
    pthread_t threadIDs[MAX_NUM_OUTPUT_QUEUES]; 
    threadArgs_t threadArgs[MAX_NUM_OUTPUT_QUEUES];
    pthread_mutex_t locks[MAX_NUM_OUTPUT_QUEUES];
    queue_t queues[MAX_NUM_OUTPUT_QUEUES];
    size_t finished[MAX_NUM_OUTPUT_QUEUES];
}output_t;
output_t output;

// flag used to start moving packets - used by alarm functions
int startFlag;

// flag used to end algorithm - used by alarm functions
int endFlag; 

static __inline__ tsc_t rdtsc(void);
void set_thread_props(int tgt_core);
void assign_to_zero();
void sig_alrm(int signo);
void alarm_init();
void alarm_start();

#endif
