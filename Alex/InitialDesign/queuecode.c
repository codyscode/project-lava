#define _GNU_SOURCE

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

#define ONE_SECOND_MILI 1000000
#define MAX_NUM_THREADS 16
#define BUFFERSIZE 100

//Packets to be passed between queues
//flow (int) - tells which flow the packet corresponds to
//order (int) - tells which position in the flow the packet is
typedef struct Packet{
    int flow;
    int order;
} packet_t;

//Data structure for Queue
//core (int) - core that the thread is assigned to
//thread (int) - thread number
//last_written (size_t) - the last written packet to the queue
//data (Data) - a packet
typedef struct Queue {
    size_t lastWritten;
    packet_t data[BUFFERSIZE];
}queue_t;

//Arguments to be passed to threads
//function (void *)()- Pointer to functions for the thread
//args (int *) - pointer to all arguments for the functions
typedef struct threadArgs{
    void (*function)();
    void *args;
}threadArgs_t;

void *input_thread(void *args){
    //Get argurments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;
    void (*inputFunction)(int *) = inputArgs->function;
    void *funcArgs = inputArgs->args;

    //-------Input Thread Functionality-------
    //Rename array for better understanding/cast it to an queue_t struct
    queue_t *inputQueue = (queue_t *)funcArgs;

    //Generate initial buffer using 5 flows
    int flowNum[] = {0, 0, 0, 0, 0};
    int currFlow;
    for(int i = 0; i < BUFFERSIZE; i++){
        currFlow = (rand() % 5) + 1;
        (*inputQueue).data[i].flow = currFlow;
        (*inputQueue).data[i].order = flowNum[currFlow - 1];
        flowNum[currFlow - 1]++;
    }

    //Continuously generate input numbers until the buffer fills up, wait for the buffer to have space before writing again
    int index = 0;
    while(1){
        currFlow = (rand() % 5) + 1;
        //If the queue spot is filled then wait for the space to become available
        while((*inputQueue).data[index].flow != 0){
            ;
        }
        //Create the new packet
        (*inputQueue).data[index].flow = currFlow;
        (*inputQueue).data[index].order = flowNum[currFlow - 1];
        //Update the last known packet place
        (*inputQueue).lastWritten = index;
        //Update the next available position in the flow
        flowNum[currFlow - 1]++;
        //Update the next spot to be written in the queue
        index++;
        index = index % BUFFERSIZE;
    }
    //Any Addtional Functions for threads use this:
    //(*inputFunction)(funcArgs);
}

void *output_thread(void *args){
    //Get argurments and any other functions for output threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;
    void (*outputFunction)(int *) = outputArgs->function;
    int *funcArgs = outputArgs->args;

    //-------Output Thread Functionality-------


    //Any Addtional Functions for threads use this:
    //(*outputFunction)(funcArgs);
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
        printf("Usage: Queues <# input queues>, <# output queues>\n");
        exit(1);
    }
    //Grab the number of input and output queues to use
    int inputQueuesCount = atoi(argv[1]);
    int outputQueuesCount = atoi(argv[2]);

    //Make sure that the number of input and output queues is valid
    assert(inputQueuesCount + outputQueuesCount <= MAX_NUM_THREADS);
    assert(inputQueuesCount > 0 && outputQueuesCount > 0);

    //Print out testing information to the user
    printf("\nTesting with: \n%d Input Queues \n%d Output Queues\n", inputQueuesCount, outputQueuesCount);

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
        threadArgsToPass[inputIndex].args = &threadQueues[inputIndex];

        FENCE()

        //create the threads
        if(pthreadErr = pthread_create(&threadIDs[inputIndex], &attrs, input_thread, &threadArgsToPass[inputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
    }
    /*
    //create the output threads
    int outputIndex;
    void *outputFuncPtr = &outputFunction;
    for(outputIndex = 0; outputIndex < outputQueuesCount; outputIndex++){
        int trueOutputIndex = outputIndex + inputIndex;
        
        //Assigned function and arguments for the funciton for the output threads
        threadArgsToPass[trueOutputIndex].function = outputFuncPtr;
        threadArgsToPass[trueOutputIndex].args = &threadQueues[trueOutputIndex];

        FENCE()

        if(pthreadErr = pthread_create(&threadIDs[trueOutputIndex], &attrs, output_thread, &threadArgsToPass[trueOutputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
    }
    */

    int currIndices[inputQueuesCount];
    //Set all start indices to 0 initially
    for(int i = 0; i < inputQueuesCount; i++){
        currIndices[i] = 0;
    }
    //Array used for checking if the packets are in the appropriate order
    //[Thread index][flow index]
    int nextExpectedOrder[inputQueuesCount][5 * inputQueuesCount];
    //set all intial expected values to 0
    for(int i = 0; i < inputQueuesCount; i++){
        for(int j = 0; j < 5 * inputQueuesCount; j++){
            nextExpectedOrder[i][j] = 0;
        }
    }

    int reads = 0;
    int maxReads = 2000;
    //Master thread that reads out the input queues to make sure they can be read in order
    while(reads < maxReads){
        //Wait for queue to fill up
        usleep(20000); // 20ms
        //Read each input queue and output results to confirm they are in order
        for(int threadIndex = 0; threadIndex < inputQueuesCount; threadIndex++){
            int index = currIndices[threadIndex];
            //While there is data to read in the queue, then read it
            while(threadQueues[threadIndex].data[index].flow != 0){
                int currFlow = threadQueues[threadIndex].data[index].flow;
                //If the order is out of line (i.e the flow number is not what was expected) then break
                if(nextExpectedOrder[threadIndex][currFlow] != threadQueues[threadIndex].data[index].order){
                    printf("Packet out of order\n");
                    exit(1);
                }
                //Otherwise increase next expected order number for the next packet in the flow
                else{
                    nextExpectedOrder[threadIndex][threadQueues[threadIndex].data[index].flow]++;
                }
                //Process the packet
                printf("Thread: %d | Flow: %d | Order: %d\n", threadIndex, threadQueues[threadIndex].data[index].flow, threadQueues[threadIndex].data[index].order);
                //Tell the queue that the position is free
                threadQueues[threadIndex].data[index].flow = 0;
                //Increment to the next spot to check in the curent queue
                index++;
                index = index % BUFFERSIZE;
                //Increase the number of reads
                reads++;
                if(reads > maxReads){
                    break;
                }
            }
            if(reads < maxReads){
                printf("No data currently available moving to next thread\n");
                currIndices[threadIndex] = index;
            }
            else{
                break;
            }
        }
    }
    printf("-------------FINISHED-------------\nPackets are in correct order\n");
}