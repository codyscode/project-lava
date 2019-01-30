#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>

/*
Assumptions about structure:
- Each queue is a thread
- Max 8 input and 8 output threads
- Queue is a queue of packet structs with 2 members: address and size
- specific threads for input and output
- "master" thread that reads data from queues for analytics
*/

#define ONE_SECOND_MILI 1000000
#define MAX_NUM_THREADS 16
#define BUFFERSIZE 256

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
    void *function;
    int *args;
}threadArgs;

void *input_thread(threadArgs *args){
    threadArgs *inputArgs = (threadArgs *)args;
    void *inputFunction = inputArgs->function;
    int *funcArgs = inputArgs->args;
    while(1){
        (*inputFunction)(*funcArgs);
    }
}

void *output_thread(threadArgs *args){
    threadArgs *outputArgs = (threadArgs *)args;
    void *outputFunction = outputArgs->function;
    int *funcArgs = outputArgs->args;
    while(1){
        (*outputFunction)(*funcArgs);
    }
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

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //create the input threads
    int pthreadErr;
    int inputIndex;
    for(inputIndex = 0; inputIndex < inputQueuesCount; inputIndex++){
        //Set the schedule specifically, so it doesnt inherit from the main thread
        if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
            perror("pthread_attr_setinheritsched");
	    }
        //Assigned function and arguments for the funciton for the output threads
        threadArgs args;

        //create the threads
        if(pthreadErr = pthread_create(threadIDs[inputIndex], &attrs, input_thread, &threadQueues[inputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }

    }
    //create the output threads
    int outputIndex;
    for(outputIndex = inputIndex; outputIndex < outputQueuesCount; outputIndex++){
        //Set the schedule specifically, so it doesnt inherit from the main thread
        if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
            perror("pthread_attr_setinheritsched");
	    }

        //Assigned function and arguments for the funciton for the output threads
        threadArgs args;

        if(pthreadErr = pthread_create(threadIDs[outputIndex], &attrs, output_thread, &args)){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
    }


    printf("Testing with: \n%d Input Queues \n%d Output Queues\n", inputQueuesCount, outputQueuesCount);
    printf("%d  aa\n", '8');
}