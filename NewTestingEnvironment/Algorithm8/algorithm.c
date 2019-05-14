/*
In this algorithm, each input thread writes packets contiguously to a local 
buffer creating a vector. The full vector is memcpyd to shared memory. An output
thread then memcpys the vector to its local buffer where it's processed. Even 
though each byte gets memcpyd twice as many times, it's significantly faster.
Presumably because caches aren't thrashing.

This builds off of algorithm 6 and segments the buffer into 2 separate buffers
The algorithm can be sped up by unraveling the for loop, however it is currently like
this to allow testing of the optimal number of segments
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "BitmaskDBContiguousVectors"

//This buffer size had the best throughput but if latency is considered it could be 
//adjusted. For example, buffers half this size were only 1 Gbps slower for 8 to 8.
#define BUFFSIZEBYTES 65536

typedef struct VBSeg{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
} vbseg_t;

typedef struct VBQueue{
    size_t paddingF[8];
    vbseg_t seg[2];
    size_t paddingR[8];
} vbqueue_t;

vbqueue_t queues[MAX_NUM_OUTPUT_THREADS][MAX_NUM_INPUT_THREADS];

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
    for (int i = 0; i < MAX_NUM_OUTPUT_THREADS; i++){
        for (int j = 0; j < MAX_NUM_INPUT_THREADS; j++){
            for(int k = 0; k < 2; k ++){
                queues[i][j].seg[k].ptr = queues[i][j].seg[k].buffer;
            }
        }
    }
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;
    size_t threadIndex = inputArgs->threadNum;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Pointer to the output threads shared buffer that it pulls packets from
    vbseg_t* shared1;

    //Initialize all local queues
    vbseg_t * local = (vbseg_t *)Malloc(sizeof(vbseg_t) * outputThreadCount);
    for(size_t i = 0; i < outputThreadCount; i++){
        local[i].ptr = local[i].buffer;
    }

    //Which segment we are currently writing for a given queue
    size_t segIndex[MAX_NUM_INPUT_THREADS] = {0};

    //Compute the correct bit mask for flows based on the number of output queues
    //This finds the next largest power of two - 1. (i.e 5 -> (8 - 1), 11 -> (16 - 1));
    //Works for only 64 bit numbers
    size_t mask = outputThreadCount;
    mask--;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    mask |= mask >> 32;

    //The corresponding output thread's shared queue to write to
    size_t qIndex = 0;
    size_t maxIndex = outputThreadCount - 1;

    //Dummy data to copy
    unsigned char data[MAX_PAYLOAD_SIZE];

    //Keep track of next order number for a given flow
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow;
    size_t currLength;
    size_t offset = inputArgs->threadNum * FLOWS_PER_THREAD;
	
    //Used for generating random numbers
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    //Signal that this thread is ready
    input[inputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration writes a packet to the local buffer, when the local buffer
    //is full the entire vector is copied to the shared buffer.
    while(1){
        while(1){
            // *** START PACKET GENERATOR ***
            //Min value: offset || Max value: offset + 7
            seed0 = (214013 * seed0 + 2531011);   
            currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

            //Min value: 64 || Max value: 8191 + 64
            seed1 = (214013 * seed1 + 2531011); 
            currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
            // *** END PACKET GENERATOR  ***

            qIndex = (currFlow & mask);
            if(qIndex > maxIndex)
                qIndex = qIndex >> 1;

            memcpy(local[qIndex].ptr, &currFlow, 8);
            local[qIndex].ptr += 8;
            memcpy(local[qIndex].ptr, &currLength, 8);
            local[qIndex].ptr += 8;
            memcpy(local[qIndex].ptr, &orderForFlow[currFlow - offset], 8);
            local[qIndex].ptr += 8;
            memcpy(local[qIndex].ptr, data, currLength);
            local[qIndex].ptr += currLength;

            //Update the next flow number to assign
            orderForFlow[currFlow - offset]++;
            
            //If we don't have room in the local buffer for another packet it's time to memcpy to shared memory.
            //A timeout could be added for real-world situations where few packets are coming in and local buffers
            //take a long time to fill.
            //For as fast as possible, this will almost always skip the while loop
            if ((local[qIndex].ptr - local[qIndex].buffer + MAX_PACKET_SIZE) >= BUFFSIZEBYTES) {
                shared1 = &queues[qIndex][threadIndex].seg[segIndex[qIndex]];

                //If there's still data in the shared buffer, wait
                while (shared1->ptr > shared1->buffer) {
                    ;
                }

                //Copy the entire vector to shared memory
                memcpy(shared1->buffer, local[qIndex].buffer, (local[qIndex].ptr - local[qIndex].buffer));

                //Signal to output_thread there's more data in shared memory and how much
                shared1->ptr =shared1->buffer + (local[qIndex].ptr - local[qIndex].buffer);

                //Reset the local queue
                local[qIndex].ptr = local[qIndex].buffer;

                //Cycle between which segment we are writing to
                segIndex[qIndex] ^= 1;
                break;
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

    //Pointer to the output threads shared buffer that it pulls packets from
    vbseg_t* shared1;

    //Which segment we are currently reading from for a given sub section of the queue
    size_t segIndex[MAX_NUM_INPUT_THREADS] = {0};

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
        //Go through each block in its corresponding queue.
        //The number of blocks is equal to the number of input threads
        for(size_t i = 0; i < inputThreadCount; i++){
            shared1 = &queues[qIndex][i].seg[segIndex[i]];

            //Wait until more data has been written to shared memory
            if (shared1->ptr == shared1->buffer) {
                continue;
            }
            else{
                //Flip which segment we are reading from
                segIndex[i] ^= 1;
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
                memcpy(&packet, readPtr, ((packet_t*) readPtr)->length + 24);

                //Packets order must be equal to the expected order.
                if(expected[packet.flow] != packet.order){
                    //Print out the contents of the local buffer that caused an error
                    int index = 0;
                    unsigned char* indexPtr = local.buffer;
                    while (indexPtr < local.ptr) {
                        packet_t * errPacket = (packet_t*) indexPtr;
                        printf("\nPosition: %d, Flow: %ld, Order: %ld", index, errPacket->flow, errPacket->order);
                        index++;
                        indexPtr += (errPacket->length + 24);
                    }

                    //Print out the specific packet that caused the error to the user
                    printf("\nError Packet: Flow %lu | Order %lu\n", packet.flow, packet.order);
                    printf("Packet out of order in thread: %lu. Expected %lu | Got %lu\n", outputArgs->threadNum, expected[packet.flow], packet.order);
                    exit(0);
                }
                else{              
                    //Set what the next expected packet for the flow should be
                    expected[packet.flow]++;

                    //Move readPtr to address of next packet
                    readPtr += (packet.length + 24);
                }
            }

            //At the end of this loop all packets in the local buffer have been processed and we update byteCount
            output[outputArgs->threadNum].byteCount += (readPtr - local.buffer);
        }
    }

    return NULL;
}

pthread_t * run(void *argsv){

    initializeCustomQueues();

    return NULL;
}