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
#define INIT_NUM_BUFFERS 10
#define FREE 0
#define NOT_FREE 1

typedef struct VQueue vqueue_t;

//The buffer struct that is used by the threads to communicate
//buffer (unsigned char array) - Allows contiguous blocks of packets
//ptr (unsigned char *) - Points to where we are currently in the buffer
//next (vqueue_t **) - points to the next address that points to the next queue
//                     this is a double pointer to allow spin locks to work
//                     can be a single pointer in the case where the output
//                     thread sleeps instead of spinnign
struct VQueue{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
    vqueue_t ** next;
};

//The head of the shared queues that each input thread appends to
vqueue_t ** sharedHeads[MAX_NUM_INPUT_THREADS];

//The head of the free list of buffers that the input thread can choose to fill
vqueue_t ** freeHeads[MAX_NUM_INPUT_THREADS];

//The tails of the free list of buffers to allow the output threads to append to when they are done
vqueue_t ** freeTails[MAX_NUM_INPUT_THREADS];

unsigned char resetPtr;

char* get_name(){
    return ALGNAME;
}

function get_input_thread(){
    return input_thread;
}

function get_output_thread(){
    return output_thread;
}

//Generate the buffers to be used and add them all to the free list initially
void generate_buffers(){
    vqueue_t ** curr;
    vqueue_t * end = NULL;

    //Generate the initial number of buffers and pin the head and tail to the appropriate locations
    //for each input thread
    for (int i = 0; i < inputThreadCount; i++){
        //Assign the head to the first buffer
        curr = (vqueue_t **)Malloc(sizeof(vqueue_t *));
        *curr = (vqueue_t *)Malloc(sizeof(vqueue_t));
        freeHeads[i] = curr;

        //Generate the middle buffers
        for(int j = 0; j < INIT_NUM_BUFFERS - 1; j++){
            (**curr).next = (vqueue_t **)Malloc(sizeof(vqueue_t *));
            *((**curr).next) = (vqueue_t *)Malloc(sizeof(vqueue_t));
            curr = (**curr).next;
        }
        //Assign the tail to the last buffer
        freeTails[i] = curr;
        (**curr).next = &end;
    }

    return;
}

//Generate the buffers to be used and add them all to the free list initially
void init_shared(){
    //Have the initial heads point to empty buffers
    for (int i = 0; i < inputThreadCount; i++){
        sharedHeads[i] = (vqueue_t **)Malloc(sizeof(vqueue_t *));
        *sharedHeads[i] = NULL;
    }
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Each input thread is assigned a single shared queue to append buffers to
    vqueue_t ** sharedList = sharedHeads[inputArgs->threadNum];
    vqueue_t ** freeListHead = freeHeads[inputArgs->threadNum];

    //Create a pointer to the first buffer we start with
    vqueue_t * local = *(freeListHead);
    freeListHead = (**freeListHead).next;
    *(local->next) = NULL;

    //Set the ptr to the beginning of the buffer to begin writing
    local->ptr = local->buffer;

    //Used to write packets to local buffer
    packet_t packet;

    //Keep track of next order number for a given flow
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow; 
    size_t currLength;

    //Used for generating unique flows between threads for validation purposes
    size_t offset = inputArgs->threadNum * FLOWS_PER_THREAD;
	
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    //Signal that the thread is ready to begin writing
    input[inputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //The first iteration in the loop is different from the rest
    //Each iteration writes a packet to the local buffer, when the local buffer
    //is full the entire vector is appended to the shared list
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
        memcpy(local->ptr, &packet, currLength + PACKET_HEADER_SIZE);

        //Update local ptr to the next address we'll write a packet
        local->ptr += (currLength + PACKET_HEADER_SIZE);

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;
        
        //If we don't have room in the local buffer for another packet it's time to memcpy to shared memory.
        //Because we are appending to a linked list, we dont need to use any locking here as the output thread
        //ensures proper access
        if ((local->ptr - local->buffer + MAX_PACKET_SIZE) >= BUFFSIZEBYTES) {
            //Append the buffer to the shared list to increase the list the output thread has to process
            *sharedList = local;
            sharedList = (**sharedList).next;

            //Find the next free buffer from the free list to fill up.
            //if the free list is empty, then malloc a buffer.
            //else use the next available buffer
            //*** IMPORTANT ***
            //The idea behind this is to avoid locking on the input side. However, this assumes the output side is
            //faster than the input side. If it were not the cae, there will be a memory leak to the order of magnitude equiavalent
            //to how must faster the input side is compared to the output side.
            if(*freeListHead == NULL){
                local = (vqueue_t *)Malloc(sizeof(vqueue_t));
                local->next = (vqueue_t **)Malloc(sizeof(vqueue_t *));
                *(local->next) = NULL;
            }
            else{
                local = *freeListHead;
                freeListHead = (**freeListHead).next;
                *(local->next) = NULL;
            }
            //Reset the local queue to start working on it again
            local->ptr = local->buffer;
            sleep(1);
        }
    }

    return NULL;
}

void * output_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    set_thread_props(outputArgs->coreNum, 2);

    //Start on the first shared list of buffers this output thread is reponsible for
    size_t qIndex = outputArgs->threadNum;

    vqueue_t ** shared[MAX_NUM_INPUT_THREADS];
    vqueue_t * freeList[MAX_NUM_INPUT_THREADS];

    //Each output thread is assigned up to 8 shared lists, that input queues append to, to manage
    //Set the pointers for each shared queue this output thread manages
    size_t sIndex = 0;
    for(; qIndex < inputThreadCount; qIndex += outputThreadCount){
        shared[sIndex] = sharedHeads[qIndex];
        sIndex++;
    }

    //After an output thread is finished with copying its buffer it puts it back on the free list
    size_t fIndex = 0;
    qIndex = outputArgs->threadNum;
    for(; qIndex < inputThreadCount; qIndex += outputThreadCount){
        freeList[fIndex] = *freeTails[qIndex];
        fIndex++;
    }

    qIndex = outputArgs->threadNum;

    //Initialize local queue that we will use to process locally away from the free queue to allow
    //the free buffers to be freed faster
    vqueue_t local;

    //Used to read packets from local buffer
    packet_t packet;

    //Points to the current packet in the local buffer
    unsigned char *readPtr;

    //Used to verify order for a given flow
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 

    //Signal that this output thread is ready to begin
    output[outputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration copies a full vector from shared memory and processes it.
    while(1){
        //This allows the output thread to manage multiple input thread lists and cycle through them without
        //processing one that another output thread is processing
        for(size_t sharedIndex = qIndex; sharedIndex < inputThreadCount; sharedIndex += outputThreadCount){
            //Wait until there is another buffer to process on the list
            while (*shared[sharedIndex] == NULL){
                ;
            }

            //Copy the entire vector from shared to local memory
            memcpy(local.buffer, (*shared[sharedIndex])->buffer, ((*shared[sharedIndex])->ptr - (*shared[sharedIndex])->buffer));

            //local.ptr marks where data ends in the buffer
            local.ptr = local.buffer + ((*shared[sharedIndex])->ptr - (*shared[sharedIndex])->buffer);

            //Put the shared buffer back on the free list as we are done with it and it can be written to again
            freeList[sharedIndex]->next = shared[sharedIndex];
            freeList[sharedIndex] = *(freeList[sharedIndex]->next);

            //Move to the next pointer pointing to the next buffer
            shared[sharedIndex] = (*shared[sharedIndex])->next;

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
                        printf("\nPosition: %d, Flow: %ld, Order: %ld", index, errPacket->flow, errPacket->order);
                        index++;
                        indexPtr += (errPacket->length + PACKET_HEADER_SIZE);
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
                    readPtr += (packet.length + PACKET_HEADER_SIZE);
                }
            }
            //At the end of this loop all packets in the local buffer have been processed and we update byteCount
            output[outputArgs->threadNum].byteCount += (readPtr - local.buffer);
        }
    }
    return NULL;
}


pthread_t * run(void *argsv){
    generate_buffers();
    init_shared();

    return NULL;
}