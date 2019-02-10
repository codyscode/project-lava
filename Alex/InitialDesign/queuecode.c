/*
Code made by Alex Widmann
Code Snippets taken from example-queue.c in example code folder written by Ian Wells @ Cisco
Date Last Modified: 2/6/2019
Purpose: Simulate passing packets from n input queues to m output queues
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>

/*
Simulating Algorithm in -> Simulation: Worload 1 - Algorithm - Workload 2

Structure of Implementation:
-Each queue has a thread associated with it,
 to avoid having to lock the output queues to write to them
-Max 8 input and 8 output threads
-Queue is a queue of packet structs with 2 members: flow and order
-specific threads for input and output
-Flows are confined to single vectors
-Can be thought of as spraying vectors to output queues
-------------CAUSES VECTORS TO STAY TOGETHER BUT FLOWS ACCROSS VECTORS CAN GO TO DIFFERENT OUTPUT QUEUES----------------

-Basic idea:
    -Input queues (threads) continuously write to themsselves and have a last read member
     where output queues pick up at for reading.
    -Input queues (threads) only write to a position that has a 0 for 
     flow(indicating there is no "packet" there).
    -Output Theads cycle through all input queues,
     where each method to take packets from an input queue has a lock on it
    -Once an output thread claims a lock it grabs packets according to a random vector size
    -For grabbing packets, the output queue writes 0 to flow member in the input queue when it grabs a packet,
     indicating its free, and writes the packet to a position based on the last written member for the 
     queue struct
    -After the output thread grabs all the packets for the vector it releases the lock
     and starts "processing" the packets
        -Processing only consists of checking if the packets are in the proper order.

    -After processing the output thread goes back into the cycle of looking for an input queue
     that is not locked

-Input Queues:
    -The input queues dont experience race conditions with output queues as the input queues only write to a position once it free
     and stays at the same position, waiting (constantly checking in a while loop) until its free before it writes again.
    -The thought process for having the input queues fill as fast as possible is that the input queues (workload 1) are not the bottle neck,
     so they should operate in the worst condition possible a.k.a filling up with as much data as fast as possible

-Output Queues:
    -The output queus dont experience race conditions as each output queue is assigned to a thread so only one thread ever reads an output queu
    -The output threads dont experience race conditions against each other for accessing input queues because of locks
    -The output threads dont experience race conditions against the input threads for the input queues because it reads until it hits a 0
    or it reads until it hits the vector size.
    -As of now the output threads are responsible for grabbing packets and procesing packets (Which should be the job for workload 2).
     The output threads have to have time taken away from grabbing packets.

-Timing:
    -As of now timing is not implemented:
        -Planned implementation: Use the TSC (time stamp counter):
            -Record TSC result when the output thread is cycling looking for an input queue to claim
            -Record TSC result when the output thread unlocks and starts processing
            -Take the difference during the processing stage and store result in a global variable
            -Repeat until main process stops

        -Reasoning:
            -Simulates timing only for the algorithm itself.
            -When the output starts processing we want to record when it starts as processing would
             be the job of workload 2 which is does not affect the algorithm performance in the worst case
             (Worst case is output consumes as soon as we give input, no delay for algorithm).
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

#define MAX_NUM_QUEUES 16
#define MAX_NUM_THREADS 20
#define MAX_NUM_INPUT_QUEUES 8
#define MAX_NUM_OUTPUT_QUEUES 8
#define BUFFERSIZE 1000
#define RUNTIME 5000000
#define CAL_DUR 2000000ULL
#define CALIBRATION 5

//Packets to be passed between queues:
//flow (int) - tells which flow the packet corresponds to
//order (int) - tells which position in the flow the packet is
typedef struct Packet{
    int flow;
    unsigned int order;
} packet_t;

//Data structure for Queue:
//toRead (unsigned int) - The next unread position in the queue
//toWrite (unsigned int) - The next position to write to in the queue
//count (unsigned long) - The number of packets read by the queue
//data (packet_t array - Space for packets in the queue
typedef struct Queue {
    unsigned int toRead;
    unsigned int toWrite;
    unsigned long count;
    packet_t data[BUFFERSIZE];
}queue_t;

//Arguments to be passed to input threads
//queue (queue_t) - queue for the input thread to write to
//queueNum (unsigned int) - The queue number relative to other queues in the arrays of queues
typedef struct inputThreadArgs{
    queue_t *queue;
    unsigned int queueNum;
}inputThreadArgs_t;

//Arguments to be passed to output threads
//queue (queue_t) - queue for the input thread to write to
//inputQueueCount (int) - The number of input queues, so the output threads know how large the cycle is
typedef struct outputThreadArgs{
    queue_t *queue;
    int inputQueueCount;
}outputThreadArgs_t;

//Initialize items for threads
pthread_t threadIDs[MAX_NUM_THREADS]; 
queue_t queues[MAX_NUM_QUEUES];
pthread_mutex_t locks[MAX_NUM_QUEUES];
inputThreadArgs_t inputThreadArgsToPass[MAX_NUM_INPUT_QUEUES];
outputThreadArgs_t outputThreadArgsToPass[MAX_NUM_OUTPUT_QUEUES];

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

    //Print the loading bar
    printLoading(i);
    return total;
}

void *input_thread(void *args){
    //Get argurments and any other functions for input threads
    inputThreadArgs_t *inputArgs = (inputThreadArgs_t *)args;
    int queueNum = inputArgs->queueNum;
    queue_t *inputQueue = (queue_t *)inputArgs->queue;

    //Each input buffer has 5 flows associated with it that it generates
    int flowNum[] = {0, 0, 0, 0, 0};
    int currFlow;
    int offset = queueNum * 5;

    //Generate initial buffer so it is fillled up to its max
    for(int i = 0; i < BUFFERSIZE; i++){
        currFlow = ((rand() % 5) + 1) + offset;
        (*inputQueue).data[i].flow = currFlow;
        (*inputQueue).data[i].order = flowNum[currFlow - 1 - offset];
        flowNum[currFlow - 1 - offset]++;
    }

    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will continuosly check that entry until it is open
    int index = 0;
    while(1){
        currFlow = ((rand() % 5) + 1) + offset;

        //If the queue spot is filled then wait for the space to become available (flow = 0). Continuously check to avoid stall
        while((*inputQueue).data[index].flow != 0){
            ;
        }

        //Create the new packet
        (*inputQueue).data[index].flow = currFlow;
        (*inputQueue).data[index].order = flowNum[currFlow - 1 - offset];

        //Update the next available position in the flow
        flowNum[currFlow - 1 - offset]++;

        //Update the next spot to be written in the queue
        index++;
        index = index % BUFFERSIZE;
    }
}

void processPackets(queue_t *outputQueue, int vectorSize){
    //Proccess packets to confirm they are in the correct order before consuming more. 
    //Similar to how master thread code in main method executes
    int expected[] = {-1, -1, -1, -1, -1};
    int index = (*outputQueue).toRead;
    int count = 0;

    //Go through each packet in the output queue until we reach an emtpy space in which case we are done
    while((*outputQueue).data[index].flow > 0){
        //Adjust for index as the flows are all within continuous groups of 5 (i.e 1, 2, 3, 4, 5 or 6, 7, 8, 9, 10)
        int currFlow = ((*outputQueue).data[index].flow - 1) % 5;

        //If the initial index doesnt exist, then the first packet of a flow is given a pass
        if(expected[currFlow] == -1){
            //Keep track of how many packets have passed through queue
            (*outputQueue).count++;

            //Process the packet
            expected[currFlow] = (*outputQueue).data[index].order + 1;
            (*outputQueue).data[index].flow = 0;
            count++;
            index++;
            index = index % BUFFERSIZE;
        }
        //If a packet is grabbed out of order exit
        else if(expected[currFlow] != (*outputQueue).data[index].order){
            printf("Flow %d | Order %d\n", (*outputQueue).data[index].flow, (*outputQueue).data[index].order);
            printf("Packet out of order. Expected %d | Got %d\n", expected[currFlow], (*outputQueue).data[index].order);
            exit(1);
        }
        //Else "process" the packet by filling in 0 and incrementing the next expected
        else{            
            (*outputQueue).count++;

            //Process the packet
            //Keep track of how many packets have passed through queue
            (*outputQueue).data[index].flow = 0;
            expected[currFlow]++;
            count++;
            index++;
            index = index % BUFFERSIZE;
        }
        if(count > vectorSize){
            printf("Processed too many packets. Count: %d, Vector Size: %d\n", count, vectorSize);
            exit(1);
        }
    }
    //printf("Successfully Processed %ld Packets on thread %02x, Looking for next queue...\n", (*outputQueue).count, pthread_self());
}

void *output_thread(void *args){
    //Get argurments and any other functions for output threads
    outputThreadArgs_t *outputArgs = (outputThreadArgs_t *)args;
    int queueCount = outputArgs->inputQueueCount;
    queue_t *outputQueue = (queue_t *)outputArgs->queue;
    int inputQueueIndex = 0;
    int vectorSize;
    tsc_t totalTime = 0;
    tsc_t startTime, finishTime;

    //Have each queue cycle through all queues trying to find an unlocked one to take from.
    //Generate a random number to simulate a random vector size
    while(1){
        //Vector size between 100 and 256 packets
        vectorSize = (rand() % 156) + 100;
        //Start recording time again
        startTime = rdtsc();

        //Continuously cycle to find an input queue that is not locked to take packets from
        while(1){
            //If the lock is successful then do work.
            //pthread_mutex_trylock returns 0 if successful in finding a lock thats not locked
            //It additionally claims the lock
            if(!pthread_mutex_trylock(&locks[inputQueueIndex])){
                //Rename queue for easier reading
                queue_t inputQueue = queues[inputQueueIndex];

                //Get starting indices
                int inputIndex = inputQueue.toRead;
                int outputIndex = (*outputQueue).toWrite;

                //Get the position to start processing packets after reading them in
                (*outputQueue).toRead = (*outputQueue).toWrite;

                //Consume (write to its queue) <vectorSize> # of packets from chosen input queue
                for(int packet = 0; packet < vectorSize; packet++){
                    //If there isnt a packet in the next spot to read, then break as it hasnt been generated yet
                    if(inputQueue.data[inputIndex].flow == 0){
                        break;
                    }
                    //else take the packet out and set flow member within struct to 0 to indicate its free
                    else{
                        (*outputQueue).data[outputIndex].flow = inputQueue.data[inputIndex].flow;
                        (*outputQueue).data[outputIndex].order = inputQueue.data[inputIndex].order;
                        inputQueue.data[inputIndex].flow = 0;
                        outputIndex++;
                        outputIndex = outputIndex % BUFFERSIZE;
                        (*outputQueue).toWrite = outputIndex;
                        inputIndex++;
                        inputIndex = inputIndex % BUFFERSIZE;
                        inputQueue.toRead = inputIndex;
                    }
                }

                //Unlock the queue so it can "process" the packets (Processing = making sure the flows are in order; the vector is in order)
                //break out of inner while loop afterwards to get a new vector size to grab
                pthread_mutex_unlock(&locks[inputQueueIndex]);
                //compute the total time the output queue took to grab that vector
                finishTime = rdtsc();
                totalTime += (finishTime - startTime);
                //"Process" the packets
                processPackets(outputQueue, vectorSize);
                break;
            }

            //If the queue is not available move on to try the next queue
            else{
                inputQueueIndex++;
                inputQueueIndex = inputQueueIndex % queueCount;
            }
        }
        //Once the output queue goes through a set number of packets then exit
        if((*outputQueue).count > RUNTIME){
            printf("Output Queue Finished, Processed %ld packets Successfully in %f Seconds\n", (*outputQueue).count, (double)totalTime/clockSpeed);
            return NULL;
        }
    }
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
    assert(inputQueuesCount <= MAX_NUM_INPUT_QUEUES);
    assert(outputQueuesCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(inputQueuesCount > 0 && outputQueuesCount > 0);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Get a baseline for the number of ticks in a second
    clockSpeed = cal();

    //initialize all queues
    for(int queueIndex = 0; queueIndex < inputQueuesCount + outputQueuesCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            queues[queueIndex].data[dataIndex].flow = 0;
            queues[queueIndex].data[dataIndex].order = 0;
        }
    }
    printf("Initialized All Queues..\n");

    //create the input generation threads
    int pthreadErr;
    int mutexErr;
    for(int inputIndex = 0; inputIndex < inputQueuesCount; inputIndex++){
        //initialize all the locks
        if(mutexErr = pthread_mutex_init(&locks[inputIndex], NULL)){
            fprintf(stderr, "pthread_mutex_init: %d\n", mutexErr);
        }

        //Assigned function and arguments for the funciton for the output threads
        inputThreadArgsToPass[inputIndex].queue = &queues[inputIndex];
        inputThreadArgsToPass[inputIndex].queueNum = inputIndex;

        //Initialize Queues
        queues[inputIndex].toRead = 0;
        queues[inputIndex].toWrite = 0;

        //Make sure data is written
        FENCE()

        //create the threads
        if(pthreadErr = pthread_create(&threadIDs[inputIndex], &attrs, input_thread, (void *)&inputThreadArgsToPass[inputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
    }
    
    //create the output processing threads
    int outputIndex;
    int trueOutputIndex;
    for(outputIndex = 0; outputIndex < outputQueuesCount; outputIndex++){
        trueOutputIndex = outputIndex + inputQueuesCount;
        
        //Assigned function and arguments for the funciton for the output threads
        outputThreadArgsToPass[trueOutputIndex].queue = &queues[trueOutputIndex];
        outputThreadArgsToPass[trueOutputIndex].inputQueueCount = inputQueuesCount;
         
        //Initialize Queues
        queues[trueOutputIndex].toRead = 0;
        queues[trueOutputIndex].toWrite = 0;

        //Make sure data is written
        FENCE()

        if(pthreadErr = pthread_create(&threadIDs[trueOutputIndex], &attrs, output_thread, (void *)&outputThreadArgsToPass[trueOutputIndex])){
            fprintf(stderr, "pthread_create: %d\n", pthreadErr);
        }
    }
    

    //Print out testing information to the user
    printf("\nTesting with: \n%d Input Queues \n%d Output Queues\n\n", inputQueuesCount, outputQueuesCount);
    
    //Run until the program for a certain amount of time based on RUNTIM variable
    for(int queueIndex = inputQueuesCount; queueIndex < outputQueuesCount + inputQueuesCount; queueIndex++){
        pthread_join(threadIDs[queueIndex], NULL);
    }
    

    /*
    //------SINGLE THREAD FOR CONFIRMING VALID INPUT-------
    int currIndices[inputQueuesCount];
    //Set all start indices for the position in the queue to read to 0 initially
    for(int i = 0; i < inputQueuesCount; i++){
        currIndices[i] = 0;
    }
    //Array used for checking if the packets are in the appropriate order
    //[Thread index][flow index]
    int nextExpectedOrder[inputQueuesCount][5 * inputQueuesCount + 1];
    //set all intial expected values to 0
    for(int i = 0; i < inputQueuesCount; i++){
        for(int j = 0; j < 5 * inputQueuesCount; j++){
            nextExpectedOrder[i][j] = 0;
        }
    }

    int reads = 0;
    int maxReads = 10000;
    //Master thread that reads out the input queues to make sure they can be read in order
    while(reads < maxReads){
        //Wait for queue to fill up
        usleep(20000); // 20ms
        //Read each input queue and output results to confirm they are in order
        for(int queuesIndex = 0; queuesIndex < inputQueuesCount; queuesIndex++){
            int index = currIndices[queuesIndex];
            //While there is data to read in the queue, then read it
            while(queues[queuesIndex].data[index].flow != 0){
                int currFlow = queues[queuesIndex].data[index].flow;
                //If the order is out of line (i.e the flow number is not what was expected) then break
                if(nextExpectedOrder[queuesIndex][currFlow] != queues[queuesIndex].data[index].order){
                    printf("Packet out of order. Expected %d | Got %d\n", nextExpectedOrder[queuesIndex][currFlow], queues[queuesIndex].data[index].order);
                    exit(1);
                }
                //Otherwise increase next expected order number for the next packet in the flow
                else{
                    nextExpectedOrder[queuesIndex][currFlow]++;
                }
                //Process the packet
                printf("Queue: %d | Flow: %d | Order: %d\n", queuesIndex, queues[queuesIndex].data[index].flow, queues[queuesIndex].data[index].order);
                //Tell the queue that the position is free
                queues[queuesIndex].data[index].flow = 0;
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
                currIndices[queuesIndex] = index;
            }
            else{
                break;
            }
        }
    }
    printf("-------------FINISHED-------------\nPackets are in correct order\n");
    */
}