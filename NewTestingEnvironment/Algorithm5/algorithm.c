/*
Created By: Alex Widmann
Last Modified: 3 June 2019

-   This algorithm takes the static approach with queue mappings defined
-   in algorithm 3, but allows input threads to write to multiple output
-   queues making an onto mapping. The function is onto which means the 
-   output side can have multiple input threads mapped to it and the same
-   goes for input to output. This means a small overhead is saved per
-   packet when the number of input threads is greater or equal to the
-   number of output threads, but the overhead is included again when the
-   number of output threads is greater than the number of input threads.
-   This allows output threads to not be wasted in every case of m x n 
-   threads. It uses one set of queues that the input threads write to 
-   directly and the output threads read from. Packets are grouped into 
-   vectors before being passed to output threads. and as soon as it is 
-   placed in the buffer and the output thread is ready to read it, the 
-   packet is processed.

-   Notes: queue and buffer share the same definition in the comments
*/
#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm5"

//The middle man queues with a maximum of max(# input threads, # output threads)
queue_t mainQueues[MAX_NUM_INPUT_THREADS];

//Base queue to write to and the max queue to write to for the input side
size_t inputBaseQueues[MAX_NUM_INPUT_THREADS];
size_t inputNumQueues[MAX_NUM_INPUT_THREADS];

//Base queue to read from and the max queue to read from for the output side
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
-   When the number of input threads is greater than or equal to the
-   number of output threads, each input thread generates a packet and
-   writes it to its corresponding queue that the thread is assigned at
-   creation. This static mapping allows extremely fast packet passing 
-   at the cost of unused output threads. When the number of output
-   threads is strictly less than the number of input threads, a hash
-   function is implemented to route packets to their appropriate queue.

-   When the buffer the input thread is attempting to write to is full 
-   it sits on a spin lock until the buffer is free to write to again. 
-   Due to testing for absolute performance we used spinlocks to avoid 
-   context switches as much as possible. In a real world scenario this
-   would be using semaphores or other sleep based locking methods.

-   Each input thead generates 5 flows:
-   (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
-   The flows can be scrambled coming from a single input:
-   (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)

-   Packets are written one at a time to the appropriate queue and written
-   to an array meaning that there can be empty space between packets if
-   the payload isnt of max size.
*/
void * input_thread(void * args){
    //Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = inputArgs->threadNum;   
    set_thread_props(inputArgs->coreNum, 2);

    //Dummy data to write to the packet
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //The number of queues that this input thread is writing to
    size_t numQueuesMan = inputNumQueues[threadNum];

    //The base and limit queues that the thread should write to
    size_t baseQueueIndex = inputBaseQueues[threadNum];

    //Each input buffer has a certain number flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};

    //Temporary variables that allow superscalar execution
    size_t currFlow, currLength;

    //Used to index into the queue struct
    size_t qIndex = 0;
    size_t dataIndex = 0;

    //The offset to not allow duplicate flows
    //Ex: Thread 1 generates flows: 1, 2, 3, 4 ...
    //    Thread 2 generates flows: 1 + offset, 2 + offset, 3 + offset ...
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    //Used for the fast random number generator
    //Used a lot for this code so it is a register variable
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    //Say this thread is ready to generate and pass
    input[threadNum].readyFlag = 1;

    //Wait until the start flag is given by the framework
    while(startFlag == 0);

    //Write packets to their corresponding queues the input thread is manageing
    while(1){
        // *** START PACKET GENERATOR ***
        //Min value: offset || Max value: offset + 7
        seed0 = (214013 * seed0 + 2531011);   
        currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

        //Get which queue the flow should go to
        qIndex = (currFlow % numQueuesMan) + baseQueueIndex;

        //Min value: 64 || Max value: 8191 + 64
        seed1 = (214013 * seed1 + 2531011); 
        currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
        // *** END PACKET GENERATOR  ***

        //get the next available index to write the packet to
        dataIndex = mainQueues[qIndex].toWrite;

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while(mainQueues[qIndex].data[dataIndex].isOccupied == OCCUPIED){
            ;//Do Nothing until the queue is free to write to
        }

        //Write the packet data to the queue
        memcpy(&mainQueues[qIndex].data[dataIndex].packet.payload, packetData, currLength);
        mainQueues[qIndex].data[dataIndex].packet.order = orderForFlow[currFlow - offset];
        mainQueues[qIndex].data[dataIndex].packet.flow = currFlow;
        mainQueues[qIndex].data[dataIndex].packet.length = currLength;

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;

        //Say that the segment is ready to be read and move onto the next queue it is managing
        mainQueues[qIndex].data[dataIndex].isOccupied = OCCUPIED;

        //Move to the next data index in the queue
        mainQueues[qIndex].toWrite++;
        mainQueues[qIndex].toWrite %= BUFFERSIZE;
            
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

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 

    //The number of queues that this input thread is writing to
    size_t numQueuesMan = outputNumQueues[threadNum];

    //The base and limit queues that the thread should write to
    size_t baseQueueIndex = outputBaseQueues[threadNum];
    size_t limitQueueIndex = baseQueueIndex + numQueuesMan;

    //Used to index into queue structs
    size_t qIndex = baseQueueIndex;
    size_t dataIndex;

    //Used to allow superscalar operations
    size_t currFlow;

    //Packet data
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //Say this thread is ready to process
    output[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Go through an entire output queue and consume all packets
    while(1){
        //Get the data index for the queue
        dataIndex = mainQueues[qIndex].toRead;

        //If there is no packet to read then move to the next queue it is managing
        if(mainQueues[qIndex].data[dataIndex].isOccupied == NOT_OCCUPIED){
            qIndex++;
            if(qIndex >= limitQueueIndex)
                qIndex = baseQueueIndex;
            continue;
        }

        //Get the current flow for the packet
        currFlow = mainQueues[qIndex].data[dataIndex].packet.flow;

        //Get the length of the payload of the packet
        //currLength = mainQueues[qIndex].data[dataIndex].packet.length;
        
        //Packets order must be equal to the expected order.
        //Implementing less than currflow causes race conditions with writing
        //Any line that starts with a * is ignored by python script
        if(expected[currFlow] != mainQueues[qIndex].data[dataIndex].packet.order){
            //Print out the specific packet that caused the error to the user
            printf("\nError Packet: Flow %lu | Order %lu\n", mainQueues[qIndex].data[dataIndex].packet.flow,mainQueues[qIndex].data[dataIndex].packet.order);
            printf("Packet out of order in Output thread: %lu at output queue %lu. Expected %lu | Got %lu\n", threadNum, qIndex, expected[currFlow], mainQueues[qIndex].data[dataIndex].packet.order);
            exit(1);
        }    

        //Pull the data out of the packet
        memcpy(packetData, mainQueues[qIndex].data[dataIndex].packet.payload, mainQueues[qIndex].data[dataIndex].packet.length);

        //increment the number of bits passed
        output[threadNum].byteCount += mainQueues[qIndex].data[dataIndex].packet.length + PACKET_HEADER_SIZE;

        //Set what the next expected packet for the flow should be
        expected[currFlow]++;

        //Say that the queue is ready to be written to again
        mainQueues[qIndex].data[dataIndex].isOccupied = NOT_OCCUPIED;
        
        //Move to the next data index to read from in the queue
        mainQueues[qIndex].toRead++;
        if(mainQueues[qIndex].toRead >= BUFFERSIZE)
            mainQueues[qIndex].toRead = 0;
    }

    return NULL;
}

void init_queues(){
    //initialize all values for built in input/output queues to 0
    for(int qIndex = 0; qIndex < MAX_NUM_INPUT_THREADS; qIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            mainQueues[qIndex].data[dataIndex].packet.flow = 0;
            mainQueues[qIndex].data[dataIndex].packet.order = 0;
            mainQueues[qIndex].data[dataIndex].packet.length = 0;
            mainQueues[qIndex].data[dataIndex].isOccupied = NOT_OCCUPIED;
        }
    }

    for(int i = 0; i < MAX_NUM_OUTPUT_THREADS; i++){
        outputBaseQueues[i] = i;
        outputNumQueues[i] = 1;
    }
}

void assignQueues(size_t numQueuesToAssign[], size_t baseQueuesToAssign[], int passerQueueCount, int threadCount){
    //Number of threads to be assigned 
    int threads = threadCount;

    //Keep track of the number of queues still to assign
    int passerQueuesCounter = passerQueueCount;

    //Which queues we are assigning
    int queueIndex = 0;

    //The ratio of remaining threads to queues stil needing to be assigned
    int threadToQueueRatio;

    //For each thread assign it a base queue and a number of queues after to read from
    for(int i = 0; i < threads; i++){
        //Get the ratio of remaining input or output threads to remaining passer queues
        threadToQueueRatio = (int)ceil((double)passerQueuesCounter / threadCount);

        //This is the number of in/out queues that the current passer queue should handle
        numQueuesToAssign[i] = threadToQueueRatio;

        //Assign the base index. Only the base is needed as we can calculate the other indexes as they are contiguous
        baseQueuesToAssign[i] = queueIndex;

        //Adjust the number of remaining number of passer queues
        //increase index of next available queue to the next free queue 
        //threads that still need to be assigned is reduced by 1
        passerQueuesCounter = passerQueuesCounter - threadToQueueRatio;
        queueIndex = queueIndex + threadToQueueRatio;
        threadCount--;
    }
}

pthread_t * run(void *argsv){
    //Get the correct number of intermediary queues
    //This number is equivalent to max(inputThreadCount, outputThreadCount)
    int passerQueueCount;

    if(inputThreadCount >= outputThreadCount){
        passerQueueCount = inputThreadCount;
    }
    else{
        passerQueueCount = outputThreadCount;
    }

    init_queues();

    //Determine which input queues go with which passing thread
    assignQueues(inputNumQueues, inputBaseQueues, passerQueueCount, inputThreadCount);

    //Determine which output queues go with which passing thread
    assignQueues(outputNumQueues, outputBaseQueues, passerQueueCount, outputThreadCount);

    return NULL;
}
