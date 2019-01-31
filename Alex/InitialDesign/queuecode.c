#define _GNU_SOURCE
#define ONE_SECOND_MILI 1000000
#define MAX_NUM_THREADS 16
#define BUFFERSIZE 256

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>

/*
Assumptions about structure:
- Each queue is a thread
- Max 8 input and 8 output threads
- Queue is a queue of packet structs with 2 members: address and size
- specific threads for input and output
- "master" thread that reads data from queues for analytics
*/


typedef unsigned long long tsc_t;

#if defined(__i386__)

typedef unsigned long long tsc_t;

// Not actually used - most chips are x64 nowadays.
static __inline__ tsc_t rdtsc(void)
{
    register tsc_t x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static __inline__ tsc_t rdtsc(void)
{
    register unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (tsc_t)lo)|( ((tsc_t)hi)<<32 );
}

#endif

#define FENCE() \
    asm volatile ("mfence" ::: "memory");

typedef struct Packet{
    int address;
    int sourcePort;
    int destPort;
    int payloadSize;
} packet_t;

//Data structure for Queue
//core (int) - core that the thread is assigned to
//thread (int) - thread number
//last_written (size_t) - the last written packet to the queue
//data (Data) - a packet
typedef struct Queue {
    int core;
    int thread;
    size_t last_written;
    packet_t data[BUFFERSIZE];
}queue_t;

typedef struct threadArgs{
    void (*function)();
    int *args;
}threadArgs_t;

void *input_thread(void *args){
    threadArgs_t *inputArgs = (threadArgs_t *)args;
    void (*inputFunction)(int *) = inputArgs->function;
    int *funcArgs = inputArgs->args;

    (*inputFunction)(funcArgs);
}

void *output_thread(void *args){
    threadArgs_t *outputArgs = (threadArgs_t *)args;
    void (*outputFunction)(int *) = outputArgs->function;
    int *funcArgs = outputArgs->args;

    (*outputFunction)(funcArgs);
}

void *inputFunction(void *args){
    int *threadNumPtr = (int *)args;
    printf("Started Input Thread: %d\n", *threadNumPtr + 1);
}

void *outputFunction(void *args){
    int *threadNumPtr = (int *)args;
    printf("Started Output Thread: %d\n", *threadNumPtr + 1);
}

int main(int argc, char **argv){
    //Error checking for proper running
    if (argc < 3){
        printf("Usage: Queues <# input queues>, <# output queues> > output.csv");
        exit(1);
    }
    //Grab the number of input and output queues to use
    int inputQueuesCount = atoi(argv[1]);
    int outputQueuesCount = atoi(argv[2]);

    //Make sure that the number of input and output queues is valid
    assert(inputQueuesCount + outputQueuesCount <= MAX_NUM_THREADS);
    assert(inputQueuesCount > 0 && outputQueuesCount > 0);

    //Initialize items for threads
    pthread_t threadIDs[MAX_NUM_THREADS]; 
    queue_t threadQueues[MAX_NUM_THREADS];
    threadArgs_t threadArgsToPass[MAX_NUM_THREADS];

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //create the input threads
    int pthreadErr;
    int inputIndex;
    void *inputFuncPtr = &inputFunction;
    for(inputIndex = 0; inputIndex < inputQueuesCount; inputIndex++){
        //Assigned function and arguments for the funciton for the output threads
        threadArgsToPass[inputIndex].function = inputFuncPtr;
        threadArgsToPass[inputIndex].args = &inputIndex;

        FENCE()

        //create the threads
        if(pthreadErr = pthread_create(&threadIDs[inputIndex], &attrs, input_thread, &threadArgsToPass[inputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
        pthread_join(threadIDs[inputIndex], NULL);
    }
    //create the output threads
    int outputIndex;
    void *outputFuncPtr = &outputFunction;
    for(outputIndex = 0; outputIndex < outputQueuesCount; outputIndex++){
        int trueOutputIndex = outputIndex + inputIndex;
        
        //Assigned function and arguments for the funciton for the output threads
        threadArgsToPass[trueOutputIndex].function = outputFuncPtr;
        threadArgsToPass[trueOutputIndex].args = &outputIndex;

        FENCE()

        if(pthreadErr = pthread_create(&threadIDs[trueOutputIndex], &attrs, output_thread, &threadArgsToPass[trueOutputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
        pthread_join(threadIDs[trueOutputIndex], NULL);
    }

    printf("\nTesting with: \n%d Input Queues \n%d Output Queues\n", inputQueuesCount, outputQueuesCount);
}