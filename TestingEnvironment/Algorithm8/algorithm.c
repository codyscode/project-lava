/*
Created By: Alex Widmann
Last Modified: 3 June 2019

-   This algorithm takes algorithm 6 and algorithm 2 and combines them
-   into one to leverage the optimizations of algorithm 6, while also
-   implementing the efficient hashing implemented in algorithm 2 to
-   allow all output threads to be used.

-   The algorithm uses 3 sets of queues where one set is shared, one set
-   belongs to only input threads and one set only applies to output
-   threads. Input threads write a vector of packets to the single local
-   queue they were assigned at created and upon filling the local queue
-   and checking if the segment in the shared queues they want to write to
-   is free, the input thread memcpys the vector over and goes back to
-   writing in its local buffer again.

-   Each thread that is assigned to an output thread is partitioned into
-   a certain number of blocks which is equal to the number of input 
-   threads. Each block is restricted to a single input thread for writing
-   allowing for less locking. each block is then partitioned into 2
-   segments to allow concurrent reads and writes for input and output
-   threads.

-   The shared buffer that each input thread copies its local buffer into
-   is defined using a static map similar to algorithm 3 to reduce the
-   overhead of flow hashing per packet.

-   Output threads wait for the current buffer to be filled with a vector
-   and then they copy it into its local buffer, marks the shared buffer 
-   as free and then starts to process the packets.

-   Notes: queue and buffer share the same definition in the comments
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm8"

//This buffer size had the best throughput but if latency is considered it could be 
//adjusted. For example, buffers half this size were only 1 Gbps slower for 8 to 8.
#define BUFFSIZEBYTES 65536

/*
struct VBSegment
-   buffer (unsigned char array) -  where all the data is written to.
-   ptr (unsigned char *) - where we currently are in the buffer.
*/
typedef struct VBSeg{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
} vbseg_t;

/*
struct VSegment
-   Padding - used to avoid invalidating other threads cache lines
-   segment (vseg_t array) - a portion of the queue. The number of
                             segments is fixed
-   Padding - used to avoid invalidating other threads cache lines
*/
typedef struct VBQueue{
    size_t paddingF[8];
    vbseg_t seg[2];
    size_t paddingR[8];
} vbqueue_t;

//Shared queues
vbqueue_t queues[MAX_NUM_OUTPUT_THREADS][MAX_NUM_INPUT_THREADS];

//Standard interface to framework for returning the algorithm name
char* get_name(){
    return ALGNAME;
}

//Standard interface to framework for returning the input method
function get_input_thread(){
    return input_thread;
}

//Standard interface to framework for returning the output method
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

/*
The job of the input threads is to make packets to populate the buffers.
As of now the packets are stored in a buffer.

Attributes:
-   Each input thread computes a bitmask to determine which queue a packet
-   should be passed to. The bitmask is applied to the flow to determine
-   which queue it should go to. Then the packet is written to the
-   appropriate block in the queue (dependent on the thread number).
-   When the buffer the intput thread is attempting to write to is full
-   it sits on a spin lock until the buffer is free to write to again. Due
-   to testing for absolute performance we used spinlocks to avoid context
-   switches as much as possible. In a real world scenario this would be
-   using semaphores or other sleep based locking methods.

-   Input threads have a number of local buffers to write to which is
-   equal to the number of output threads. The bitmask is also equivalent
-   to the number of output threads. When a local buffer fills up, that
-   vector is memcopied into the appropriate buffer in the corresponding
-   block and the output thread copies this into its local buffer to
-   process when it is ready.

-   The input thread indexes through the array using ptrs and using the
-   length member of the packet to determine the index of the next packet.

-   To increase speed, shared buffers are divided into segments so that 
-   when the number of segments is greater than 1, theoretically the 
-   input thread can write to one side of the buffer while the output 
-   thread reads from the other side. Upon testing it seems that any 
-   number of segments above 2 has no impact on performance for that 
-   buffersize listed above.
*/
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

        //Write the packet to the local buffer
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
        }
    }
    return NULL;
}

/*
The job of the processing threads is to ensure that the packets are being 
delivered in proper order by going through output queues and reading the 
order

Attributes:
-   Each output thread is assigned certain buffers to read from using
-   Base and limit numbers which allow the constraints of the problem to
-   be followed while also avoiding locking between output threads
-   When the buffer the output queue is attempting to read from is empty
-   it sits on a spin lock until the buffer is free to write to again. Due
-   to testing for absolute performance we used spinlocks to avoid context
-   switches as much as possible. In a real world scenario this would be
-   using semaphores or other sleep based locking methods.

-   Output threads check the shared memory segments, searching for one
-   with a complete vector of packets. When it is found, the output
-   thread copies the vector in shared memory to its local buffer, marks
-   the position in shared memory as free, and starts processing the
-   vector in its local buffer.

-   Each shared queue that the output thread is paritioned using a 2d 
-   array, This means all input threads are communicating (passing data) 
-   with all output threads. The output thread cycles through these 
-   partitions checking for complete vectors to process.

-   The output thread indexes through the array using ptrs and using the
-   length member of the packet to determine the index of the next packet.

-   Each output thead ensures the flows given to it are passed in order.
-   The order is checked using a static array that defines the total 
-   number threads being generated per thread times the number of input 
-   threads. This is because we dont know which flow is being passed to
-   which output thread allowing freedom.

-   If a packet is recieved out of order the corresponding thread prints
-   the out of order packet, the buffer it came from, and signals the 
-   framework to exit.
*/
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

    initializeCustomQueues();

    return NULL;
}
