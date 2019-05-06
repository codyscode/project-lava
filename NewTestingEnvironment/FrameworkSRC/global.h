#ifndef GLOBAL_H
#define GLOBAL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <signal.h>
#include <poll.h>

//Constants for upper limit of threads
#define MAX_NUM_INPUT_THREADS 8
#define MAX_NUM_OUTPUT_THREADS 8

//Minimum number of threasd required
#define MIN_INPUT_THREAD_COUNT 1
#define MIN_OUTPUT_THREAD_COUNT 1

//Buffersize for default queues
#define BUFFERSIZE 512

//Defines how many ticks an algorithm should run for
#define RUNTIME 10

//Used to determine packet size
//8000 Allows much faster packet generation
#define MIN_PAYLOAD_SIZE 64
#define MAX_PAYLOAD_SIZE 65

//Maximum packet size
#define MIN_PACKET_SIZE 24 + MIN_PAYLOAD_SIZE
#define MAX_PACKET_SIZE 24 + (MAX_PAYLOAD_SIZE - 1)

//Number of unique flows that each input thread generates
#define FLOWS_PER_THREAD 8

//Indicates whether a packet is there or not
#define NOT_OCCUPIED 0
#define OCCUPIED 1

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
typedef struct Packet{
    size_t flow; 
    size_t length;
    size_t order;
    unsigned char payload[MAX_PAYLOAD_SIZE];
}packet_t;

//Data field for the queue
typedef struct Data{
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
    data_t data[BUFFERSIZE];
}queue_t;

//Arguments to be passed to input threads
//queue (*queue_t) - pointer to the first queue for the thread to write to/process
//coreNum (size_t) - used to define which core the processing queue should be assigned to
//threadNum (size_t) - The queue number relative to other queues in its set
typedef struct threadArgs{
    queue_t *queue;
    size_t coreNum;
    size_t threadNum;
}threadArgs_t;

//threadID (pthread_t) - The Id for the thread
//threadArgs (threadArgs_t) - Arguments to be passed to input/output threads
//queue (queue_t) - built in queues for passing
//readyFlag (size_t) - Flag signaling thead is ready
//count (size_t) - amount of data passed
typedef struct io{
    threadArgs_t threadArgs;
    pthread_t threadID;
    queue_t queue;
    size_t readyFlag;
    size_t byteCount;
}io_t;

//initialize array of input and ouput threads
io_t input[MAX_NUM_INPUT_THREADS];
io_t output[MAX_NUM_OUTPUT_THREADS];

//Used for number of input and output threads
size_t inputThreadCount;
size_t outputThreadCount;

//flag used to start moving packets - used by alarm functions
int startFlag;

//flag used to end algorithm - used by alarm functions
int endFlag; 

//Total to be used for calculating packets passed
size_t finalTotal;

//Used to store total overhead for generating packets
size_t overheadTotal;

void set_thread_props(int tgt_core, long sched);
void sig_alrm(int signo);
void alarm_init();
void alarm_start();

void * input_thread(void * args);
void * output_thread(void * args);
char * get_name();
function get_input_thread();
function get_output_thread();
pthread_t * run(void * args);

#endif