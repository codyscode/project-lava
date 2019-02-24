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

//Constants for upper limit of queues
#define MAX_NUM_QUEUES 16
#define MAX_NUM_INPUT_QUEUES 8
#define MAX_NUM_OUTPUT_QUEUES 8

//Constants for threads
#define MAX_NUM_THREADS 20
#define BUFFERSIZE 256

//How many packets need to be processed by each queue before the program terminates
//We could also make this time instead
#define RUNTIME 50000

//Used for calibrating the amount of clock cycles in a second
#define CAL_DUR 2000000ULL
#define CALIBRATION 5

#define MIN_PACKET_SIZE 24
#define MAX_PACKET_SIZE 9024

#define MAX_PAYLOAD_SIZE 9000
#define FLOWS_PER_QUEUE 8

#define HEADER_SIZE 24

#define FENCE() \
   __asm__ volatile ("mfence" ::: "memory");

typedef unsigned long long tsc_t;

//Data structure to represent a packet.
//length (size_t) - The total size of the packet
//flow (size_t) - The flow of the packet
//order (size_t) - The order of the packet within its flow
//data (size_t) - Payload of the packet
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
//data (packet_t array - Space for packets in the queue
typedef struct Queue {
    size_t toRead;
    size_t toWrite;
    size_t count;
    packet_t data[BUFFERSIZE];
}queue_t;

//Arguments to be passed to input threads
//firstQueue (queue_t) - pointer to the first queue for the thread to write to/process
//firstQueueNum (size_t) - The index of the queue in the array of queues
typedef struct threadArgs{
    queue_t *queue;
    size_t queueNum;
}threadArgs_t;

//input and output struct are mostly identical right now and mainly used to
//abstract the inputs resources from the outputs resources but we may want
//to add unique characteristics to each at some point.

//Initialize items for input threads
//queueCount (size_t) - The number of input queues
//queues (queue_t) - The input queue struct
//queues (queue_t) - The input queue struct
//threadIds (pthread_t) - The Id's for ech input thread
//threadArgs (threadArgs_t) - Arguments to be passed to input threads
//locks (pthread_mutex_t) - lock for each input queue
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
//queues (queue_t) - The output queue struct
//threadIds (pthread_t) - The Id's for ech output thread
//threadArgs (threadArgs_t) - Arguments to be passed to output threads
//locks (pthread_mutex_t) - lock for each output queue
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

//Used to record roughly the number of ticks in a second
tsc_t clockSpeed;

static __inline__ tsc_t rdtsc(void);
void printLoading(int);
tsc_t cal(void); 

#endif
