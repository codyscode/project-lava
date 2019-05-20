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

#define ALGNAME "Algorithm7"

//This buffer size had the best throughput but if latency is considered it could be 
//adjusted. For example, buffers half this size were only 1 Gbps slower for 8 to 8.
#define BUFFSIZEBYTES 65536

//Number of segments for the queue
#define NUM_SEGS 2

typedef struct VBSegment{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
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
            queues[i].segment[j].ptr = queues[i].segment[j].buffer;
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
    local.ptr = local.buffer;

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
            while(1){
                // *** START PACKET GENERATOR ***
                //Min value: offset || Max value: offset + 7
                seed0 = (214013 * seed0 + 2531011);   
                currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

                //Min value: 64 || Max value: 8191 + 64
                seed1 = (214013 * seed1 + 2531011); 
                currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
                // *** END PACKET GENERATOR  ***

                //Write the packet data to the local buffer
                memcpy(local.ptr, &currFlow, 8);
                local.ptr += 8;
                memcpy(local.ptr, &currLength, 8);
                local.ptr += 8;
                memcpy(local.ptr, &orderForFlow[currFlow - offset], 8);
                local.ptr += 8;
                memcpy(local.ptr, data, currLength);
                local.ptr += currLength;

                //Update the next flow number to assign
                orderForFlow[currFlow - offset]++;
                
                //If we don't have room in the local buffer for another packet it's time to memcopy to shared memory.
                //A timeout could be added for real-world situations where few packets are coming in and local buffers
                //take a long time to fill.
                if ((local.ptr - local.buffer + MAX_PACKET_SIZE) >= BUFFSIZEBYTES) {
                    //If there's still data in the shared buffer, wait
                    while (shared1->ptr > shared1->buffer) {
                        ;
                    }
                    //Copy the entire vector to shared memory
                    memcpy(shared1->buffer, local.buffer, (local.ptr - local.buffer));

                    //Signal to output_thread there's more data in shared memory and how much
                    shared1->ptr = shared1->buffer + (local.ptr - local.buffer);

                    //Reset the local queue
                    local.ptr = local.buffer;
                    break;
                }
            }
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
    local.ptr = local.buffer;

    //Used to convert into a packet struct
    packet_t packet;

    //readPtr points to the current packet in the local buffer
    unsigned char *readPtr;

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
            while (shared1->ptr == shared1->buffer) {
                ;
            }
            //Copy the entire vector from shared to local memory
            memcpy(local.buffer, shared1->buffer, (shared1->ptr - shared1->buffer));

            //local.ptr marks where data in the local buffer ends
            local.ptr = local.buffer + (shared1->ptr - shared1->buffer);

            //Signal to input_thread that shared memory can be written to again
            shared1->ptr = shared1->buffer;

            //readPtr points to the current packet in the local buffer
            readPtr = local.buffer;

            //Process all packets in the local buffer.
            while (readPtr < local.ptr) {
                //This second memcpy arguably isn't needed since all the packet data is local to this
                //thread at this point but I didn't want there to be any confusion over whether this
                //accurately models individual packets being parsed and found that adding it doesn't
                //affect the speed.  
                memcpy(&packet, readPtr, ((packet_t*) readPtr)->length + PACKET_HEADER_SIZE);

                //Packets order must be equal to the expected order.
                if(expected[packet.flow] != packet.order){
                    //Print out the contents of the local buffer that caused an error
                    int index = 0;
                    unsigned char* indexPtr = local.buffer;
                    while (indexPtr < local.ptr) {
                        packet_t * errPacket = (packet_t*) indexPtr;
                        printf("Position: %d, Flow: %ld, Order: %ld\n", index, errPacket->flow, errPacket->order);
                        index++;
                        indexPtr += (errPacket->length + PACKET_HEADER_SIZE);
                    }

                    //Print out the specific packet that caused the error to the user
                    printf("Error Packet: Flow %lu | Order %lu\n", packet.flow, packet.order);
                    printf("Packet out of order in thread: %lu. Expected %lu | Got %lu\n", outputArgs->threadNum, expected[packet.flow], packet.order);
                    exit(0);
                }
                else{              
                    //Set what the next expected packet for the flow should be
                    expected[packet.flow]++;

                    //Move readPtr to address of next packet
                    readPtr += (packet.length + PACKET_HEADER_SIZE);
                }
            }

            //At the end of this loop all packets in the local buffer have been processed and we update byteCount
            output[outputArgs->threadNum].byteCount += (readPtr - local.buffer);
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
