/*
Created By: Alex Widmann
Last Modified: 3 June 2019

-   This algorithm takes the optimizations discovered in algorithm 6
-   and applies optimizations on top of it to increase performance.

-   The algorithm uses 3 sets of queues were one set is shared, one set
-   belongs to only input threads and one set only applies to output
-   threads. Input threads write a vector of packets to the single local
-   queue they were assigned at created and upon filling the local queue
-   and checking if the segment in the shared queues they want to write to
-   is free, the input thread memcpys the vector over and goes back to
-   writing in its local buffer again.

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

#define ALGNAME "Algorithm7"

//This buffer size had the best throughput but if latency is considered
//it could be adjusted. For example, buffers half this size were only 
//1 Gbps slower for 8 to 8.
#define BUFFSIZEBYTES 65536

//Number of segments for the queue
#define NUM_SEGS 2

/*
struct VBSegment
-   buffer (unsigned char array) -  where all the data is written to.
-   ptr (unsigned char *) - where we currently are in the buffer.
*/
typedef struct VBSegment{
    unsigned char buffer[BUFFSIZEBYTES];
    unsigned char *ptr;
} vbseg_t;

/*
struct VSegment
-   segment (vseg_t array) - a portion of the queue. The number of 
-                            portions is based on the macro NUM_SEGS
*/
typedef struct VBQueue{
    vbseg_t segment[NUM_SEGS];
} vbqueue_t;

//Custom queues that the threads read and write to
vbqueue_t queues[MAX_NUM_INPUT_THREADS];

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
    for (int i = 0; i < MAX_NUM_INPUT_THREADS; i++){
        for(int j = 0; j < NUM_SEGS; j++){
            queues[i].segment[j].ptr = queues[i].segment[j].buffer;
        }
    }
}

/*
The job of the input threads is to make packets to populate the buffers.
As of now the packets are stored in a buffer.

Attributes:
-   When the number of input threads is greater than or equal to the
-   number of output threads, each input thread generates a packet and
-   writes it to its corresponding queue that the thread is assigned at
-   creation. This static mapping allows extremely fast packet passing 
-   at the cost of unused output threads.

-   Input threads writes a vector of packets a buffer only accessable by
-   it, then waits until the segment of the buffer it is trying to copy
-   to is free to write to. It then copies the local buffer into the 
-   shared buffer, marks that buffer as free, then starts filling its
-   local buffer again.

-   Packets are written back to back in a byte array that is of size
-   BUFFSIZEBYTES. the ptr is used to index into the array. Each time
-   a packet is written we check if another packet can fit or not into
-   the buffer. If not we copy the buffer to shared memory.

-   When the buffer the input thread is attempting to write to is full 
-   it sits on a spin lock until the buffer is free to write to again. 
-   Due to testing for absolute performance we used spinlocks to avoid 
-   context switches as much as possible. In a real world scenario this
-   would be using semaphores or other sleep based locking methods.

-   Each input thead generates 5 flows:
-   (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
-   The flows can be scrambled coming from a single input:
-   (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)

-   Packets are written to in vectors so when the input thread fills a
-   buffer completely it checks to see if the shared buffer is ready to
-   be copied into.

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
	
    //Used for generating packets randomly
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    //Signal that this thread is ready to start passing
    input[inputArgs->threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Each iteration writes a packet to the local buffer, when the local buffer
    //is full the entire vector is copied to the shared buffer.
    while(1){
        //Cycle through each segment we want to write to.
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
