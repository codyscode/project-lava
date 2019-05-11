/*
- This algorithm subdivides the problem into multiple smaller problems to allow faster queue writes than having all input threads communicated with all output threads through
  many queues. This saves an additional hash of knowing where to direct a packet in any of the flows generated by a single thread.
- The idea is to treat one 8 x 4 problem to four 2 x 1 problems.
- This algorithm assumes that the output is evenly distributed amongst threads
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "FullMultWire"

//The middle man queues with a maximum of max(# input threads, # output threads)
queue_t mainQueues[MAX_NUM_INPUT_THREADS];

//Base queue to write to and the max queue to write to for the input side
size_t inputBaseQueues[MAX_NUM_INPUT_THREADS];
size_t inputNumQueues[MAX_NUM_INPUT_THREADS];

//Base queue to read from and the max queue to read from for the output side
size_t outputBaseQueues[MAX_NUM_OUTPUT_THREADS];
size_t outputNumQueues[MAX_NUM_OUTPUT_THREADS];

char* get_name(){
    return ALGNAME;
}

function get_input_thread(){
    return input_thread;
}

function get_output_thread(){
    return output_thread;
}

//The job of the input threads is to make packets to populate the buffers. As of now the packets are stored in a buffer.
//Attributes:
// - Each input thread generates a packet and writes it to its corresponding queue
// - Input threads stop writing and wait when the buffer is full and continues when it is empty
// - Each input thread generates a certain number of flows (ex: input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// - The flows can be scrambled coming from a single input (ex: input stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
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
    size_t limitQueueIndex = baseQueueIndex + numQueuesMan;

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
        currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
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

//This is the old output processing thread from the framework
//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order and making sure its correct
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
        output[threadNum].byteCount += mainQueues[qIndex].data[dataIndex].packet.length + 24;

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

    /*//Print Queue Assignments
    for(int i = 0; i < inputThreadCount; i++){
        printf("input thread: %d - base queue: %lu - num queues: %lu\n", i, inputBaseQueues[i], inputNumQueues[i]);
    }
    for(int i = 0; i < outputThreadCount; i++){
        printf("output thread: %d - base queue: %lu - num queues: %lu\n", i, outputBaseQueues[i], outputNumQueues[i]);
    }

    exit(1);
    */

    return NULL;
}