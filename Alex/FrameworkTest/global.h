#ifndef GLOBAL_H
#define GLOBAL_H

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>

//Constants for upper limit of queues
#define MAX_NUM_QUEUES 16
#define MAX_NUM_INPUT_QUEUES 8
#define MAX_NUM_OUTPUT_QUEUES 8

//Constants for threads
#define MAX_NUM_THREADS 20
#define BUFFERSIZE 256

//How many packets need to be processed by each queue before the program terminates
//We could also make this time instead
#define RUNTIME 5000000

//Used for calibrating the amount of clock cycles in a second
#define CAL_DUR 2000000ULL
#define CALIBRATION 5

#define MAX_PACKET_SIZE 9000
#define FLOWS_PER_QUEUE 5

#define FENCE() \
    asm volatile ("mfence" ::: "memory");

typedef unsigned long long tsc_t;

//Data structure to represent a packet.
//length: The size of the packet
typedef struct packet{
    size_t length;
    size_t flow;
    size_t order;
    unsigned char data[MAX_PACKET_SIZE];
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
//firstQueueNum (size_t) - The index of the queue in the array fo queues
//inputQueueCount (size_t) - The total number of input queues
//outputQueueCount (size_t) - The total number of output queues
typedef struct threadArgs{
    queue_t *queue;
    size_t queueNum;
    size_t inputQueueCount;
    size_t outputQueueCount;
}threadArgs_t;

//Arguements to be passed to run function
//inputQueueCount (size_t) - number of input queues
//outputQueueCount (size_t) - number of output queues
typedef struct algoArgs{
    size_t inputQueueCount;
    size_t outputQueueCount;
}algoArgs_t;

//Initialize items for threads
pthread_t threadIDs[MAX_NUM_THREADS]; 
threadArgs_t threadArgs[MAX_NUM_THREADS];
pthread_mutex_t locks[MAX_NUM_QUEUES];
queue_t queues[MAX_NUM_QUEUES];

//Used to record roughly the number of ticks in a second
tsc_t clockSpeed;

static __inline__ tsc_t rdtsc(void);
void printLoading(int);
tsc_t cal(void);
void run(algoArgs_t*);    

#endif