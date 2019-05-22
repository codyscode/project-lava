/*
In this algorithm, each input thread writes packets contiguously to a local 
buffer creating a vector. The full vector is memcpyd to shared memory. An output
thread then memcpys the vector to its local buffer where it's processed. Even 
though each byte gets memcpyd twice as many times, it's significantly faster.
Local writes stay in the input thread's cache until it's full, making them very
fast. There are fewer but larger memcpys through shared memory. Local reads in 
the output threads also have a full cache.
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm9"

#define BUFFSIZEBYTES 4096
#define INIT_NUM_FREE 10
#define FREE 0
#define NOT_FREE 0

typedef struct VQueue vqueue_t;

struct VQueue{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
    vqueue_t ** next;
};

//The head of the shared queues
vqueue_t queues[MAX_NUM_INPUT_THREADS];

/* ---------------- FREE LIST --------------
//The heads of the free buffers for each input queue
//Free buffers are appended to this list when the output
//queue is done with them
vqueue_t * freeBuffers[MAX_NUM_INPUT_THREADS];
*/

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
        vqueue_t * nextptr = NULL;
        queues[i].next = &nextptr;
    }
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Each input thread is assigned a single shared queue to append buffers to
    vqueue_t shared = queues[inputArgs->threadNum];

    /* ---------------- FREE LIST --------------
    //initialize the buffer free list for this thread
    vqueue_t * curr = freeBuffers[inputArgs->threadNum];
    for (int i = 0; i < MAX_NUM_INPUT_THREADS; i++){
        //Create a buffer
        vqueue_t * buffer = (vqueue_t *)Malloc(sizeof(vqueue_t));

        //Add the queue to the free list
        curr->next = buffer;

        //Move to the next queue
        curr = curr->next;
    }

    vqueue_t * toAppend = freeBuffers[inputArgs->threadNum];
    */

    //Initialize a local queue
    vqueue_t local;
    local.ptr = local.buffer;

    //Used to write packets to local buffer
    packet_t packet;

    //A shared buffer to append to the shared list
    vqueue_t * toAppend;

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
            //Choose an available shared buffer to write the packets to the queue
            toAppend = (vqueue_t *)Malloc(sizeof(vqueue_t));

            //Copy the entire vector to free shared memory location
            memcpy(toAppend->buffer, local.buffer, (local.ptr - local.buffer));
            toAppend->ptr = toAppend->buffer + (local.ptr - local.buffer);

            //Make the current buffer's next pointer point to this buffer
            //Signal to output_thread there's more data in shared memory and how much
            *(shared.next) = toAppend;

            shared = toAppend;

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

    //Set the pointers for each shared queue this output thread manages
    vqueue_t ** shared[MAX_NUM_INPUT_THREADS];
    for(int i = qIndex; i < inputThreadCount; i += outputThreadCount){
        shared[i] = queues[qIndex].next;
    }

    //Initialize local queue;
    vqueue_t local;
    local.ptr = local.buffer;

    //Used for referencing a buffer to free
    vqueue_t * toFree;

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
        for(size_t sharedIndex = qIndex; sharedIndex < inputThreadCount; sharedIndex += outputThreadCount){
            //Wait until there is another buffer to process
            while (*shared[sharedIndex] == NULL){
                ;
            }

            //Copy the entire vector from shared to local memory
            memcpy(local.buffer, (**shared[sharedIndex]).buffer, ((**shared[sharedIndex]).ptr - (**shared[sharedIndex]).buffer));

            //local.ptr marks where data in the local buffer ends
            local.ptr = local.buffer + ((**shared[sharedIndex]).ptr - (**shared[sharedIndex]).buffer);

            //Signal to input_thread that shared memory can be written to again
            (**shared[sharedIndex]).ptr = (**shared[sharedIndex]).buffer;

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

            //set a reference to this buffer to free it later
            toFree = *shared[sharedIndex];

            //Move to the next buffer in the queue for the input thread
            shared[sharedIndex] = (*shared[sharedIndex])->next;

            //Free this shared buffer
            free(toFree);
        }
    }
    return NULL;
}


pthread_t * run(void *argsv){
    initializeCustomQueues();

    return NULL;
}