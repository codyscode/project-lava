/*
Created by: Cody Cunningham

Algorithm 6 was originally intended to explore the benefits of writing packets 
contiguously in memory and what other optimizations that might enable. It was 
then discovered that creating contiguous vectors of packets in exclusive local 
buffers then copying them to and from shared memory allowed us to fully exploit 
the cache, resulting in our largest performance gain over previous 
optimizations. 

In this algorithm, each input thread writes packets contiguously to a local 
buffer creating a vector. The full vector is memcpyd to shared memory. An 
output thread then memcpys the vector to its local buffer where it's processed. 
Even though each byte gets memcpyd twice as many times, it's significantly 
faster. Since local buffers are exclusive to the thread, local writes stay in 
the input thread's cache until it's full, making them very fast. Local reads in 
the output threads also have a full cache. There are fewer but larger memcpys 
through shared memory. Writing packets contiguously insures we fill our caches 
with as much data as possible and that our memcpys don't contain any empty 
space. Input queues are merged into output queues with a simple static mapping 
(described in algorithm 4) that avoids flow hashing.
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm6"

//This buffer size had the best throughput since it matches the cache size
//on our test system but buffers half this size only performed slightly worse.
//Change this to match the cache size on your system.
#define BUFFSIZEBYTES 65536

typedef struct custom_queue_t{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
} custom_queue_t;

custom_queue_t queues[MAX_NUM_INPUT_THREADS];

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
        queues[i].ptr = queues[i].buffer;
    }
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Each input thread is assigned a single shared queue
    custom_queue_t* shared = &queues[inputArgs->threadNum];

    //Initialize a local queue
    custom_queue_t local;
    local.ptr = local.buffer;

    //Used to write packets to local buffer
    packet_t packet;

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
        // *** START PACKET GENERATOR ***
        //Min value: offset || Max value: offset + 7
        seed0 = (214013 * seed0 + 2531011);   
        currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

        //Min value: 64 || Max value: 8191 + 64
        seed1 = (214013 * seed1 + 2531011); 
        currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
        // *** END PACKET GENERATOR  ***

        //Generate a packet and write it to the local buffer
        packet.order = orderForFlow[currFlow - offset];
        packet.length = currLength;
        packet.flow = currFlow;
        memcpy(local.ptr, &packet, currLength + PACKET_HEADER_SIZE);

        //Update local.ptr to the next address we'll write a packet
        local.ptr += (currLength + PACKET_HEADER_SIZE);

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;
        
        //If we don't have room in the local buffer for another packet it's time to memcpy to shared memory.
        //A timeout could be added for real-world situations where few packets are coming in and local buffers
        //take a long time to fill.
        if ((local.ptr - local.buffer + MAX_PACKET_SIZE) >= BUFFSIZEBYTES) {
            //Wait while there's still data in the shared buffer
            while (shared->ptr > shared->buffer) {
                ;
            }
            //Copy the entire vector to shared memory
            memcpy(shared->buffer, local.buffer, (local.ptr - local.buffer));
            //Signal to output_thread there's more data in shared memory and how much
            shared->ptr = shared->buffer + (local.ptr - local.buffer);
            //Reset the local queue
            local.ptr = local.buffer;
        }
    }
    return NULL;
}

void * output_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(outputArgs->coreNum, 2);

    //Start on the first shared queue this output thread is reponsible for
    size_t qIndex = outputArgs->threadNum;

    //Points to the current shared queue this output_thread is handling
    custom_queue_t *shared;

    //Initialize local queue;
    custom_queue_t local;
    local.ptr = local.buffer;

    //Used to read packets from local buffer
    packet_t packet;

    //Points to the current packet in the local buffer
    unsigned char *readPtr;

    //Used to verify order for a given flow
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 

    output[outputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration copies a full vector from shared memory and processes it.
    while(1){
        //Output threads may have to handle more than one shared queue
        shared = &queues[qIndex];
        //Wait until more data has been written to shared memory
        while (shared->ptr == shared->buffer) {
            ;
        }
        //Copy the entire vector from shared to local memory
        memcpy(local.buffer, shared->buffer, (shared->ptr - shared->buffer));
        //local.ptr marks where data in the local buffer ends
        local.ptr = local.buffer + (shared->ptr - shared->buffer);
        //Signal to input_thread that shared memory can be written to again
        shared->ptr = shared->buffer;
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

        //Move to the next shared queue this output thread is responsible for
        qIndex = qIndex + outputThreadCount;
        if(qIndex >= inputThreadCount) {
            qIndex = outputArgs->threadNum;
        }
    }
    return NULL;
}


pthread_t * run(void *argsv){

    initializeCustomQueues();

    return NULL;
}
