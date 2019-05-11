/*
In this algorithm, each input thread writes packets contiguously to a local 
buffer creating a vector. The full vector is memcpyd to shared memory. An output
thread then memcpys the vector to its local buffer where it's processed. Even 
though each byte gets memcpyd twice as many times, it's significantly faster.
Presumably because caches aren't thrashing.
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "ContiguousVectorsTest"

//This buffer size had the best throughput but if latency is considered it could be 
//adjusted. For example, buffers half this size were only 1 Gbps slower for 8 to 8.
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

    //Dummy data to copy
    unsigned char data[MAX_PAYLOAD_SIZE];

    //Keep track of next order number for a given flow
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t * currFlow
    size_t * currLength;
    size_t offset = inputArgs->threadNum * FLOWS_PER_THREAD;
	
    register unsigned int g_seed0 = (unsigned int)time(NULL);
    register unsigned int g_seed1 = (unsigned int)time(NULL);

    input[inputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration writes a packet to the local buffer, when the local buffer
    //is full the entire vector is copied to the shared buffer.
    while(1){
        // *** FASTEST PACKET GENERATOR ***
        g_seed0 = (214013*g_seed0+2531011);   
        *currFlow = ((g_seed0>>16)&0x0007) + offset;//Min value offset + 1: Max value offset + 9:
        g_seed1 = (214013*g_seed1+2531011); 
        *currLength = ((g_seed1>>16)&0XFFFF) % (MAX_PAYLOAD_SIZE - MIN_PAYLOAD_SIZE) + MIN_PAYLOAD_SIZE;

        //Generate a packet and write it to the local buffer
        packet_t packet;
        packet.order = orderForFlow[currFlow - offset];
        packet.length = currLength;
        packet.flow = currFlow;
        memcpy(local.ptr, &packet, currLength + 24);



        memcpy(local.ptr, orderForFlow[currFlow - offset], 8);
        local.ptr += 8;
        memcpy(local.ptr, currLength, 8);
        local.ptr += 8;
        memcpy(local.ptr, currFlow, 8);
        local.ptr += 8;
        memcpy(local.ptr, data, currLength);

        //Update local.ptr to the next address we'll write a packet
        local.ptr += (currLength + 24);

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;
        
        //If we don't have room in the local buffer for another packet it's time to memcpy to shared memory.
        //A timeout could be added for real-world situations where few packets are coming in and local buffers
        //take a long time to fill.
        if ((local.ptr - local.buffer + MAX_PACKET_SIZE) >= BUFFSIZEBYTES) {
            //If there's still data in the shared buffer, wait
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
    size_t currentQueueNum = outputArgs->threadNum;  
    custom_queue_t*  shared = &queues[currentQueueNum];

    //Initialize local queue;
    custom_queue_t local;
    local.ptr = local.buffer;

    //Used to verify order for a given flow
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 

    output[currentQueueNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration copies a full vector from shared memory and processes it.
    while(1){
        
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
        unsigned char *readPtr = local.buffer;
        //Process all packets in the local buffer.
        while (readPtr < local.ptr) {

            packet_t packet;
            //This second memcpy arguably isn't needed since all the packet data is local to this
            //thread at this point but I didn't want there to be any confusion over whether this
            //accurately models individual packets being parsed and found that adding it doesn't
            //affect the speed.  
            memcpy(&packet, readPtr, ((packet_t*) readPtr)->length + 24);
  
/* Ask Alex why this was needed
            // set expected order for given flow to the first packet it sees
            if(expected[packet.flow] == 0){
                expected[packet.flow] = packet.order;
            }
 */           

            //Packets order must be equal to the expected order.
            if(expected[packet.flow] != packet.order){
                //Print out the contents of the local buffer that caused an error
                int index = 0;
                unsigned char* indexPtr = local.buffer;
                while (indexPtr < local.ptr) {
                    packet_t * errPacket = (packet_t*) indexPtr;
                    printf("Position: %d, Flow: %ld, Order: %ld\n", index, errPacket->flow, errPacket->order);
                    index++;
                    indexPtr += (errPacket->length + 24);
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
                readPtr += (packet.length + 24);

            }
        }
        //At the end of this loop all packets in the local buffer have been processed and we update byteCount
        output[outputArgs->threadNum].byteCount += (readPtr - local.buffer);

        //Move to the next shared queue this output thread is responsible for
        currentQueueNum = currentQueueNum + outputThreadCount;
        if(currentQueueNum >= inputThreadCount) {
            currentQueueNum = outputArgs->threadNum;
        }
        shared = &queues[currentQueueNum];
    }
    return NULL;
}


pthread_t * run(void *argsv){

    initializeCustomQueues();

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Tell the system we are setting the schedule for the thread, instead of inheriting
    if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
        perror("pthread_attr_setinheritsched");
    }
    return NULL;
}