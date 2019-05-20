/*
- This algorithm subdivides the problem into multiple smaller problems to allow faster queue writes than having all input threads communicated with all output threads
- It additionally uses vectors to pass packets.
    - Queues are made up of 2 vectors and each vector has 1024 packets. Initially the output thread is waiting while the input thread writes to the first vector in the queue.
    - After the input thread fills up the first vector it marks it as ready and the input thread moves onto write to the next vector while the output thread reads from the first vector
    - This process alternates back and forth allowing both input and output threads to be doing work on a single queue at once.
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm3"

#define VBUFFERSIZE 512
#define NUM_SEGS 2

//isOccupied - Whether all the data there is ready to copy or not
//data (packet_t array) - Space for packets in the queue
typedef struct VSegment {
    size_t isOccupied;
    packet_t data[VBUFFERSIZE];
}vseg_t;

//2 segments, this allows when one segment is being read, for the other to be writeen to
typedef struct VQueue {
    vseg_t segments[NUM_SEGS];
}vqueue_t;

vqueue_t mainQueues[MAX_NUM_INPUT_THREADS];
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
// -Each input queue generates a packet and writes it to its correesponding queue
// -Input queue stops writing when the buffer is full and continues when it is empty
// -Each input queue generates 5 flows (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// -The flows can be scrambled coming from a single input (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
void * input_thread(void * args){
//Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = inputArgs->threadNum;   
    set_thread_props(inputArgs->coreNum, 2);

    //Data to write to the packet
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //The 2 queues that the input thread writes to
    //Alternates between two it writes to
    size_t qIndex = threadNum;
    size_t segIndex = 0;
    size_t dataIndex = 0;

    //Each input buffer has 8 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    register size_t seed0 = (size_t)time(NULL);
    register size_t seed1 = (size_t)time(NULL);

    //Say this thread is ready to generate and pass
    input[threadNum].readyFlag = 1;

    //Wait until the start flag is given by the framework
    while(startFlag == 0);

    //Write packets to their corresponding queues
    //The loop is unraveled for quick queue traversal
    while(1){
        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
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

//This is the old output processing thread from the framework
//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order
void * output_thread(void * args){
    //Get arguments and any other functions for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = outputArgs->threadNum;   
    set_thread_props(outputArgs->coreNum, 2);

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 
    size_t qIndex;
    size_t baseQIndex = outputBaseQueues[threadNum];
    size_t maxQIndex = baseQIndex + outputNumQueues[threadNum];
    size_t segIndex = 0;
    size_t dataIndex = 0;

    //Packet data
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
