/*
Created By: Alex Widmann
Last Modified: 3 June 2019

-   This algorithm takes a static approach with queue mappings and fixes
-   a single input threads to write to a single output. The function is 
-   not 1 - 1 which means the output side can have multiple input threads
-   mapped to it, however the opposite is not true (an input thread only
-   writes to one output thread). This means a small overhead saved per
-   packet which is huge in terms of passing, but also means when the
-   number of output threads is greater than the number of input threads,
-   some output threads will recieve no packets. It uses one set of queues
-   that the input threads write to directly and the output threads read 
-   from. Packets are grouped into vecotrs before being passed to output
-   threads. and as soon as it is placed in the buffer and the output 
-   thread is ready to read it, the packet is processed.

-   Notes: queue and buffer share the same definition in the comments
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm3"

#define VBUFFERSIZE 512
#define NUM_SEGS 2

/*
struct VSegment
-   isOccupied (size_t) -  Whether all the data there is ready to copy 
-                           or not
-   data (packet_t array) - Space for packets in the queue. Size defined
                            by macro VBUFFERSIZE
*/
typedef struct VSegment {
    size_t isOccupied;
    packet_t data[VBUFFERSIZE];
}vseg_t;

/*
struct VSegment
-   segments (vseg_t array) - a portion of the queue. The number of 
-                             portions is based on the macro NUM_SEGS
*/
typedef struct VQueue {
    vseg_t segments[NUM_SEGS];
}vqueue_t;

//Shared memory space to write packets to
vqueue_t mainQueues[MAX_NUM_INPUT_THREADS];
size_t outputBaseQueues[MAX_NUM_OUTPUT_THREADS];
size_t outputNumQueues[MAX_NUM_OUTPUT_THREADS];

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

/*
The job of the input threads is to make packets to populate the buffers.
As of now the packets are stored in a buffer.

Attributes:
-   Each input thread generates a packet and writes it to its 
-   corresponding queue that the thread is assigned at creation. This
-   static mapping allows extremely fast packet passing at the cost
-   of unused output threads. When the buffer the input thread is 
-   attempting to write to is full it sits on a spin lock until the buffer
-   is free to write to again. Due to testing for absolute performance we
-   used spinlocks to avoid context switches as much as possible. In a 
-   real world scenario this would be using semaphores or other sleep 
-   based locking methods.

-   Each input thead generates 5 flows:
-   (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
-   The flows can be scrambled coming from a single input:
-   (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)

-   Packets are written to in vectors so when the output thread is done
-   with a specific buffer (buffer is completely empty) it signals to the
-   input thread and the input thread locks the buffer and writes until
-   the buffer is full and signals to the output thread.

-   To increase speed buffers are divided into segments so that when the
-   number of segments is greater than 1, theoretically the input thread
-   can write to one side of the buffer while the output thread reads from
-   the other side. Upon testing it seems that any number of segments
-   above 2 has no impact on performance for that buffersize listed above.
*/
void * input_thread(void * args){
    //Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = inputArgs->threadNum;   
    set_thread_props(inputArgs->coreNum, 2);

    //Data to write to the packet
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //The buffer that this input thread writes to and the current segment
    //index to write to
    size_t qIndex = threadNum;
    size_t segIndex = 0;
    size_t dataIndex = 0;

    //Each input buffer has 8 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    //Used to randomly generate packets and their headers
    register size_t seed0 = (size_t)time(NULL);
    register size_t seed1 = (size_t)time(NULL);

    //Say this thread is ready to generate and pass
    input[threadNum].readyFlag = 1;

    //Wait until the start flag is given by the framework
    while(startFlag == 0);

    //Write packets to their corresponding queues
    while(1){
        //If the queue spot is filled then that means the input buffer is
        //full so continuously check until it becomes open
        while(mainQueues[qIndex].segments[segIndex].isOccupied == OCCUPIED){
            ;//Do Nothing until the queue is free to write to
        }

        //Write the entire queue block
        for(dataIndex = 0; dataIndex < VBUFFERSIZE; dataIndex++){
            // *** START PACKET GENERATOR ***
            //Min value: offset || Max value: offset + 7
            seed0 = (214013 * seed0 + 2531011);   
            currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

            //Min value: 64 || Max value: 8191 + 64
            seed1 = (214013 * seed1 + 2531011); 
            currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
            // *** END PACKET GENERATOR  ***

            //Write the packet data to the queue
            memcpy(&mainQueues[qIndex].segments[segIndex].data[dataIndex].payload, packetData, currLength);
            mainQueues[qIndex].segments[segIndex].data[dataIndex].order = orderForFlow[currFlow - offset];
            mainQueues[qIndex].segments[segIndex].data[dataIndex].flow = currFlow;
            mainQueues[qIndex].segments[segIndex].data[dataIndex].length = currLength;

            //Update the next flow number to assign
            orderForFlow[currFlow - offset]++;
        }

        //Say that the segment is ready to be read and move onto the next queue it is managing
        mainQueues[qIndex].segments[segIndex].isOccupied = OCCUPIED;

        //Move to the next segment in the queue
        segIndex++;
        if(segIndex >= NUM_SEGS) 
            segIndex = 0;
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
-   it sits on a spin lock until the buffer is free to read from again. Due
-   to testing for absolute performance we used spinlocks to avoid context
-   switches as much as possible. In a real world scenario this would be
-   using semaphores or other sleep based locking methods.

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
    //Get arguments and any other functions for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = outputArgs->threadNum;   
    set_thread_props(outputArgs->coreNum, 2);

    //used to "process" packets to confirm they are in the correct order 
    //before consuming more. Processing threads process until they get 
    //to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 
    size_t qIndex;
    size_t baseQIndex = outputBaseQueues[threadNum];
    size_t maxQIndex = baseQIndex + outputNumQueues[threadNum];
    size_t segIndex = 0;
    size_t dataIndex = 0;

    //Dummy Packet data to write to
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //Say this thread is ready to process
    output[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Go through an entire output queue and consume all packets
    while(1){
        //Cycle through all the queues its managing
        for(qIndex = baseQIndex; qIndex < maxQIndex; qIndex++){
            //Wait till the queue is ready to be read from
            while(mainQueues[qIndex].segments[segIndex].isOccupied == NOT_OCCUPIED){
                ; //Wait
            }

            //Go through the entire queue as we know its full and take the packets out
            for(dataIndex = 0; dataIndex < VBUFFERSIZE; dataIndex++){
                //Get the current flow for the packet
                size_t currFlow = mainQueues[qIndex].segments[segIndex].data[dataIndex].flow;
                
                //Packets order must be equal to the expected order.
                //Implementing less than currflow causes race conditions with writing
                //Any line that starts with a * is ignored by python script
                if(expected[currFlow] != mainQueues[qIndex].segments[segIndex].data[dataIndex].order){
                    //Print out the specific packet that caused the error to the user
                    printf("\nError Packet: Flow %lu | Order %lu\n", mainQueues[qIndex].segments[segIndex].data[dataIndex].flow,mainQueues[qIndex].segments[segIndex].data[dataIndex].order);
                    printf("Packet out of order in Output Queue %lu. Expected %lu | Got %lu\n", qIndex, expected[currFlow], mainQueues[qIndex].segments[segIndex].data[dataIndex].order);
                    exit(1);
                }    
                //Get the length
                size_t currLength = mainQueues[qIndex].segments[segIndex].data[dataIndex].length;

                //Pull the data out of the packet
                memcpy(packetData, mainQueues[qIndex].segments[segIndex].data[dataIndex].payload, currLength);

                //increment the number of bits passed
                output[threadNum].byteCount += currLength + PACKET_HEADER_SIZE;

                //Set what the next expected packet for the flow should be
                expected[currFlow]++;
            }

            //Say that the queue is ready to be written to again
            mainQueues[qIndex].segments[segIndex].isOccupied = NOT_OCCUPIED;
        } 

        //Move to the next segment in the queues it is managing
        segIndex++;
        if(segIndex >= NUM_SEGS) 
            segIndex = 0;
    }

    return NULL;
}

void init_queues(){
    //initialize all values for built in input/output queues to 0
    for(int qIndex = 0; qIndex < MAX_NUM_INPUT_THREADS; qIndex++){
        for(int segIndex = 0; segIndex < NUM_SEGS; segIndex++){
            for(int dataIndex = 0; dataIndex < VBUFFERSIZE; dataIndex++){
                mainQueues[qIndex].segments[segIndex].data[dataIndex].flow = 0;
                mainQueues[qIndex].segments[segIndex].data[dataIndex].order = 0;
                mainQueues[qIndex].segments[segIndex].data[dataIndex].length = 0;
                mainQueues[qIndex].segments[segIndex].isOccupied = NOT_OCCUPIED;
            }
        }
    }

    for(int i = 0; i < MAX_NUM_OUTPUT_THREADS; i++){
        outputBaseQueues[i] = i;
        outputNumQueues[i] = 1;
    }
}

void assign_queues(size_t numQueuesToAssign[], size_t baseQueuesToAssign[], size_t passerQueueCount, size_t queueCountTracker){
    size_t passerQueueCountTracker = passerQueueCount;
    size_t queueIndex = 0;
    size_t queueToPassRatio;
    for(int i = 0; i < passerQueueCount; i++){
        //Get the ratio of remaining in/out to remaining passer queues
        queueToPassRatio = (size_t)ceil((double)queueCountTracker / passerQueueCountTracker);

        //This is the number of in/out queues that the current passer queue should handle
        numQueuesToAssign[i] = queueToPassRatio;

        //Assign the base index. Only the base is needed as we can calculate the other indexes as they are contiguous
        baseQueuesToAssign[i] = queueIndex;

        //Adjust the number of remaining in/out and passer queues
        //remaining in/out queues decrease by variable amount
        //increase index of next available queue to the next free queue 
        //remaining Passer queues always decrease by 1
        queueCountTracker = queueCountTracker - queueToPassRatio;
        queueIndex = queueIndex + queueToPassRatio;
        passerQueueCountTracker--;
    }
}

pthread_t * run(void *argsv){
    init_queues();
    if(inputThreadCount > outputThreadCount){
        assign_queues(outputNumQueues, outputBaseQueues, outputThreadCount, inputThreadCount);
    }
    return NULL;
}
