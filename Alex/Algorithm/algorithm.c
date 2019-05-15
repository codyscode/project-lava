/*
In this algorithm, each input thread writes packets contiguously to a local 
buffer creating a vector. The full vector is memcopyd to shared memory. An output
thread then memcopys the vector to its local buffer where it's processed. Even 
though each byte gets memcopyd twice as many times, it's significantly faster.
Presumably because caches aren't thrashing.

This builds off of algorithm 6 and segments the buffer into 2 separate buffers
The algorithm can be sped up by unraveling the for loop, however it is currently like
this to allow testing of the optimal number of segments
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "2ContiguousVectors1Queue"

//This buffer size had the best throughput but if latency is considered it could be 
//adjusted. For example, buffers half this size were only 1 Gbps slower for 8 to 8.
#define BUFFSIZEBYTES 256

#define OCCUPIED 1
#define NOT_OCCUPIED 0

//Number of segments for the queue
#define NUM_SEGS 2

typedef struct VBSegment{
    packet_t buffer[BUFFSIZEBYTES];
    size_t isOcc;
} vbseg_t;

typedef struct VBQueue{
    vbseg_t segment[NUM_SEGS];
} vbqueue_t;

vbqueue_t queues[MAX_NUM_INPUT_THREADS];

char* get_name(){
    return ALGNAME;
}

function get_input_thread(){
    return input_thread;
}

function get_output_thread(){
    return output_thread;
}

void initializeCustomQueues(){
    for (int i = 0; i < MAX_NUM_INPUT_THREADS; i++){
        for(int j = 0; j < NUM_SEGS; j++){
            queues[i].segment[j].isOcc = NOT_OCCUPIED;
        }
    }
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    size_t threadIndex = inputArgs->threadNum;

    //Each input thread is assigned a single shared queue
    vbseg_t* shared1;

    //Initialize a local queue
    vbseg_t local;

    //Dummy data to copy
    unsigned char data[MAX_PAYLOAD_SIZE];

    //Keep track of next order number for a given flow
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow;
    size_t currLength;
    size_t offset = inputArgs->threadNum * FLOWS_PER_THREAD;
	
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    input[inputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration writes a packet to the local buffer, when the local buffer
    //is full the entire vector is copied to the shared buffer.
    while(1){
        for(size_t i = 0; i < NUM_SEGS; i++){
            shared1 = &queues[threadIndex].segment[i];
            for(size_t index = 0; index < BUFFSIZEBYTES; index++){
                // *** START PACKET GENERATOR ***
                //Min value: offset || Max value: offset + 7
                seed0 = (214013 * seed0 + 2531011);   
                currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

                //Min value: 64 || Max value: 8191 + 64
                seed1 = (214013 * seed1 + 2531011); 
                currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
                // *** END PACKET GENERATOR  ***

                //Write the packet data to the local buffer
                //memcpy(local.ptr, &currFlow, 8);
                local.buffer[index].flow = currFlow;
                //memcpy(local.ptr, &currLength, 8);
                local.buffer[index].length = currLength;
                //memcpy(local.ptr, &orderForFlow[currFlow - offset], 8);
                local.buffer[index].order = orderForFlow[currFlow - offset];
                memcpy(&local.buffer[index].payload, &data, currLength);

                //Update the next flow number to assign
                orderForFlow[currFlow - offset]++;
            }

            //If there's still data in the shared buffer, wait
            while (shared1->isOcc == OCCUPIED) {
                ;
            }
            //Copy the entire vector to shared memory
            memcpy(shared1->buffer, local.buffer, BUFFSIZEBYTES * sizeof(packet_t));

            //Signal to output_thread there's more data in shared memory
            shared1->isOcc = OCCUPIED;
        }
    }
    return NULL;
}

void * output_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(outputArgs->coreNum, 2);

    size_t qIndex = outputArgs->threadNum;

    //Start on the first shared queue this output thread is reponsible for
    vbseg_t* shared1;

    //Local variable version of the global variables
    size_t numOutput = outputThreadCount;
    size_t numInput = inputThreadCount;

    //Initialize local queue;
    vbseg_t local;

    //Used to verify order for a given flow
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 

    output[outputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration copies a full vector from shared memory and processes it.
    while(1){
        for(size_t i = 0; i < NUM_SEGS; i++){
            //Get a local variable for readability
            shared1 = &queues[qIndex].segment[i];

            //Wait until more data has been written to shared memory
            while (shared1->isOcc == NOT_OCCUPIED) {
                ;
            }

            //Copy the entire vector from shared to local memory
            memcpy(local.buffer, shared1->buffer, BUFFSIZEBYTES * sizeof(packet_t));

            //Signal to input thread this is free to write to
            shared1->isOcc = NOT_OCCUPIED;

            //Process all packets in the local buffer.
            for(size_t index = 0; index < BUFFSIZEBYTES; index++){
                //Packets order must be equal to the expected order.
                if(expected[local.buffer[index].flow] != local.buffer[index].order){
                    //Print out the contents of the processing queue that caused an error
                    for(int i = 0; i < BUFFSIZEBYTES; i++){
                        printf("Position: %d, Flow: %ld, Order: %ld\n", i, local.buffer[index].flow, local.buffer[index].order);
                    }

                    //Print out the specific packet that caused the error to the user
                    printf("Error Packet: Flow %lu | Order %lu\n", local.buffer[index].flow, local.buffer[index].order);
                    printf("Packet out of order in thread: %lu. Expected %lu | Got %lu\n", outputArgs->threadNum, expected[local.buffer[index].flow], local.buffer[index].order);
                    exit(0);
                }
                else{              
                    //Set what the next expected packet for the flow should be
                    expected[local.buffer[index].flow]++;
                    
                    //At the end of this loop all packets in the local buffer have been processed and we update byteCount
                    output[outputArgs->threadNum].byteCount += local.buffer[index].length + 24;
                }
            }
        }

        //Move to the next shared queue this output thread is responsible for
        qIndex = qIndex + numOutput;
        if(qIndex >= numInput) {
            qIndex = outputArgs->threadNum;
        }
    }

    return NULL;
}

pthread_t * run(void *argsv){

    initializeCustomQueues();

    return NULL;
}