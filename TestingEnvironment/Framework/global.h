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

//threadID (pthread_t) - The Id for the thread
//threadArgs (threadArgs_t) - Arguments to be passed to input threads
//lock (pthread_mutex_t) - lock for the queue
//queue (queue_t) - a queue struct
//finished (size_t) - flag that indicates when a given thread has generated x number of packets
typedef struct io{
    pthread_t threadID;
    threadArgs_t threadArgs;
    pthread_mutex_t lock;
    queue_t queue;
    size_t finished;
}io_t;

//initialize input and ouput arrays
io_t input[MAX_NUM_INPUT_QUEUES];
io_t output[MAX_NUM_OUTPUT_QUEUES];

size_t inputQueueCount;
size_t outputQueueCount;



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
