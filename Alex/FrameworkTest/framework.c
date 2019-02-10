#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>

#include<algorithm.h>

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
//secondQueue (queue_t) - pointer to the second queue for the thread to write to/process
//secondQueueNum (size_t) - The index of the queue in the array of queues
//inputQueueCount (size_t) - The total number of input queues
//outputQueueCount (size_t) - The total number of output queues
typedef struct threadArgs{
    queue_t *firstQueue;
    size_t firstQueueNum;
    queue_t *secondQueue;
    size_t secondQueueNum;
    size_t inputQueueCount;
    size_t outputQueueCount;
}threadArgs_t;

//Initialize items for threads
pthread_t threadIDs[MAX_NUM_THREADS]; 
threadArgs_t threadArgs[MAX_NUM_THREADS];
pthread_mutex_t locks[MAX_NUM_QUEUES];
queue_t queues[MAX_NUM_QUEUES];


//Used to record roughly the number of ticks in a second
tsc_t clockSpeed = 0;

//Purely For Visual Asthetics For Loading Bar
void printLoading(int perc){
    //Delete previous loading bar
    for(int backCount = 0; backCount < CALIBRATION * 2 + 8; backCount++){
        printf("\b");
    }

    //Print next loading bar
    printf("{");
    int prog;
    for(prog = 0; prog < (perc + 1) * 2; prog++){
        printf("#");
    }
    for(int j = prog; j < (CALIBRATION + 1) * 2; j++){
        printf("-");
    }

    //Print Percentage
    printf("}%3d%%", (perc*100)/ CALIBRATION);
}

// Find out roughly how many TSC cycles happen in a second.
// This isn't perfectly accurate - we could get more accuracy by running
// for a longer time and by running this test repeatedly.
tsc_t cal(void){
    //Notify user program is running
    printf("Normalizing Metric...\t");
    printf("{");
    for(int i = 0; i < CALIBRATION * 2; i++){
        printf("-");
    }
    printf("}%3d%%", 0);
    fflush(NULL);

    // Find a rough value for the number of TSC ticks in 1s
    tsc_t total = 0;
    int i;
    for(i = 0; i < CALIBRATION; i++){
        tsc_t start=rdtsc();
        usleep(CAL_DUR);
        tsc_t end=rdtsc();
        total += ((end-start)*1000000ULL)/(CAL_DUR * CALIBRATION);
        printLoading(i);
        fflush(NULL);
    }
    printLoading(i);
    return total;
}

//The job of the input threads is to make packets to populate the buffers. As of now the packets are stored in a buffer.
//--Need to work out working memory simulation for packet retrieval--
//Attributes:
// -Each input queue generates packets in a circular buffer
// -Input queue stops writing when the buffer is full and continues when it is empty
// -Each input queue generates 5 flows (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// -The flows can be scrambled coming from a single input (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
void* input_thread(void *args){
    //Get argurments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    int firstQueueNum = inputArgs->firstQueueNum;
    int secondQueueNum = inputArgs->secondQueueNum;
    int queueCount = 0;
    queue_t *inputQueues[2];

    //Assign the queues to an array
    if(inputArgs->firstQueue != NULL){
        inputQueues[0] = (queue_t *)inputArgs->firstQueue;
        queueCount++;
    }
    if(inputArgs->secondQueue != NULL){
        inputQueues[1] = (queue_t *)inputArgs->secondQueue;
        queueCount++;
    }

    //Each input buffer has 5 flows associated with it that it generates
    int flowNums[2][FLOWS_PER_QUEUE];
    int currFlow;
    int offsets[2] = {firstQueueNum * FLOWS_PER_QUEUE, secondQueueNum * FLOWS_PER_QUEUE};

    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will switch the other queue and start filling it
    int index[2] = {0, 0};
    int qIndex = 0;
    while(1){
        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = (rand() % FLOWS_PER_QUEUE) + offsets[qIndex] + 1;

        //If the queue spot is filled then that means the input buffer is full so switch to the other queue and start writing
        if((*(inputQueues[qIndex])).data[index[qIndex]].flow != 0){
            qIndex++;
            qIndex = qIndex % queueCount;
            continue;
        }
        //Create the new packet
        (*(inputQueues[qIndex])).data[index[qIndex]].flow = currFlow;
        (*(inputQueues[qIndex])).data[index[qIndex]].order = flowNums[qIndex][currFlow - offsets[qIndex] - 1];

        //Update the next available position in the flow
        flowNums[qIndex][currFlow - offsets[qIndex] - 1]++;

        //Update the next spot to be written in the associated queue
        index[qIndex]++;
        index[qIndex] = index[qIndex] % BUFFERSIZE;
    }
}

//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order
//--Need to work out storing in memory for proper simulation--
void* processing_thread(void *args){
    //Get argurments and any other functions for input threads
    threadArgs_t *processArgs = (threadArgs_t *)args;

    int firstQueueNum = processArgs->firstQueueNum;
    int secondQueueNum = processArgs->secondQueueNum;
    int queueCount = 0;
    int numFlows = (int)(processArgs->inputQueueCount);
    queue_t *processQueues[2];

    //Assign the queues to an array
    if(processArgs->firstQueue != NULL){
        processQueues[0] = (queue_t *)processArgs->firstQueue;
        queueCount++;
    }
    if(processArgs->secondQueue != NULL){
        processQueues[1] = (queue_t *)processArgs->secondQueue;
        queueCount++;
    }

    //"Proccess" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    int expected[numFlows * FLOWS_PER_QUEUE]; 
    int index[2] = {0, 0};
    int qIndex = 0;

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(1){
        //If there is no packet, switch over to other queue and start processing other queue
        if((*(processQueues[qIndex])).data[index[qIndex]].flow <= 0){
            qIndex++;
            qIndex = qIndex % queueCount;
            continue;
        }
        //Get the current flow for the packet
        int currFlow = (*(processQueues[qIndex])).data[index[qIndex]].flow;

        //If a packet is grabbed out of order exit
        if(expected[currFlow] != (*(processQueues[qIndex])).data[index[qIndex]].order){
            printf("Flow %lu | Order %lu\n", (*(processQueues[qIndex])).data[index[qIndex]].flow, (*(processQueues[qIndex])).data[index[qIndex]].order);
            printf("Packet out of order. Expected %d | Got %lu\n", expected[currFlow], (*(processQueues[qIndex])).data[index[qIndex]].order);
            exit(1);
        }
        //Else "process" the packet by filling in 0 and incrementing the next expected
        else{            
            //Increment how many packets the queue has processed
            (*(processQueues[qIndex])).count++;

            //Keep track of how many packets have passed through queue
            (*(processQueues[qIndex])).data[index[qIndex]].flow = 0;

            //Set what the next expected packet for the flow should be
            expected[currFlow]++;

            //Move to the next spot in the outputQueue
            index[qIndex]++;
            index[qIndex] = index[qIndex] % BUFFERSIZE;
        }
    }
    //printf("Successfully Processed %ld Packets in queue %d and %d, Looking for next queue...\n", (*(processQueues[0])).count + (*(processQueues[1])).count,\
            firstQueueNum, secondQueueNum);
}


void main(int argc, char**argv){
    //Error checking for proper running
    if (argc < 3){
        printf("Usage: Queues <# input queues>, <# output queues>\n");
        exit(1);
    }

    //Grab the number of input and output queues to use
    int inputQueueCount = atoi(argv[1]);
    int outputQueueCount = atoi(argv[2]);
    int totalQueueCount = inputQueueCount + outputQueueCount;
    //Compute cieling of the number of queues divided by 2 to find the number of threads needed
    int inputThreadCount = (inputQueueCount - 1) / 2 + 1;
    int processThreadCount = (outputQueueCount - 1) / 2 + 1;
    int totalThreadCount = inputThreadCount + processThreadCount;

    //Make sure that the number of input and output queues is valid
    assert(inputQueueCount <= MAX_NUM_INPUT_QUEUES);
    assert(outputQueueCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(inputQueueCount > 0 && outputQueueCount > 0);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Get a baseline for the number of ticks in a second
    clockSpeed = cal();

    //initialize all queues
    for(int queueIndex = 0; queueIndex < inputQueueCount + outputQueueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            queues[queueIndex].data[dataIndex].flow = 0;
            queues[queueIndex].data[dataIndex].order = 0;
        }
        queues[queueIndex].toRead = 0;
        queues[queueIndex].toWrite = 0;
    }

    //Initialize all locks
    int mutexErr;
    for(int index = 0; index < MAX_NUM_QUEUES; index++){
        if(mutexErr = pthread_mutex_init(&locks[index], NULL)){
            fprintf(stderr, "pthread_mutex_init: %d\n", mutexErr);
        }
    }

    //create the input generation threads and output processing threads.
    //Total number of threads created = (# of input queues + # of output queues) / 2
    //Determine the optimal number of threads so that there arent too many
    //As of now for every thread there are 2 queues associated with it: The number of the queue (n) and n + 1
    int pthreadErr;
    int queueIndex = 0;
    for(int index = 0; index < totalThreadCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        threadArgs[index].firstQueue = &queues[queueIndex];
        threadArgs[index].firstQueueNum = queueIndex;
        threadArgs[index].inputQueueCount = inputQueueCount;
        threadArgs[index].outputQueueCount = outputQueueCount;

        //Assign second queue and spawn input threads
        if(index <= inputThreadCount){
            //Used for if there are an odd number of queues
            if(queueIndex + 1 < inputQueueCount){
                threadArgs[index].secondQueue = &queues[queueIndex + 1];
                threadArgs[index].secondQueueNum = queueIndex + 1;
            }
            else{
                threadArgs[index].secondQueue = NULL;
                threadArgs[index].secondQueueNum = -1;
            }

            //Move to next 2 queues
            queueIndex++;

            //Make sure data is written
            FENCE()

            if(pthreadErr = pthread_create(&threadIDs[index], &attrs, input_thread, (void *)&threadArgs[index])){
                fprintf(stderr, "pthread_create: %d\n", pthreadErr);
            }
        }
        //Assign second queue and Spawn output threads
        else{
            //Used for if there are an odd number of queues
            if(queueIndex + 1 < totalQueueCount){
                threadArgs[index].secondQueue = &queues[queueIndex + 1];
                threadArgs[index].secondQueueNum = queueIndex + 1;
            }
            else{
                threadArgs[index].secondQueue = NULL;
                threadArgs[index].secondQueueNum = -1;
            }

            //Move to next 2 queues
            queueIndex++;

            //Make sure data is written
            FENCE()

            if(pthreadErr = pthread_create(&threadIDs[index], &attrs, processing_thread, (void *)&threadArgs[index])){
                fprintf(stderr, "pthread_create: %d\n", pthreadErr);
            }
        }
    }
    //Print out testing information to the user
    printf("\nTesting with: \n%d Input Queues \n%d Output Queues\n\n", inputQueueCount, outputQueueCount);

    //The algorithm portion that gets inserted into the code
    //--Another option is to move the global variables/non input/output thread functions into a different header function <global.h>--
    //--and put that header in both the framework and algorithm code--
    //--Allows for better abstraction--

    //The run function takes as arguments:
    // -pointer to queues array
    // -the number of queues
    // -the number of threads
    run();
}