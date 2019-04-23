/*
-This algorithm creates a single "wire".
-The wire works by grabbing the same amount of packets from each input queues then empties them into the appropriate output queue and it repeats this indefinitely
-The algorithm uses 3 sets of queues, One input, One wire, and One Output queue
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Lots O' Queues"

queue_t mainQueues[MAX_NUM_INPUT_THREADS * MAX_NUM_OUTPUT_THREADS];

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
    unsigned char packetData[9000];

    //Queues that the input thread writes to
    size_t baseQueueIndex = threadNum * outputThreadCount;
    size_t maxQueueIndex = baseQueueIndex + outputThreadCount;

    //Each input buffer has 5 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_QUEUE] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_QUEUE;
	
    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    size_t dataIndex = 0;
    size_t qIndex = 0;
    register unsigned int g_seed0 = (unsigned int)time(NULL);
    register unsigned int g_seed1 = (unsigned int)time(NULL);

    //Say this thread is ready to generate and pass
    input[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Write packets to their corresponding queues
    while(endFlag == 0){
        // *** FASTEST PACKET GENERATOR ***
        g_seed0 = (214013*g_seed0+2531011);   
        currFlow = ((g_seed0>>16)&0x0007) + offset;//Min value: offset || Max value: offset + 7:
        g_seed1 = (214013*g_seed1+2531011); 
        currLength = ((g_seed1>>16)&0x1FFF) + MIN_PAYLOAD_SIZE; //Min value: 64 || Max value: 8191 + 64:

        //Determine which queue to write the packet data to
        qIndex = (currFlow % (maxQueueIndex - baseQueueIndex)) + baseQueueIndex;
        dataIndex = mainQueues[qIndex].toWrite;

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while(mainQueues[qIndex].data[dataIndex].isOccupied == OCCUPIED){
            ;//Do Nothing until a space is available to write
        }

        //Write the packet data to the queue
        memcpy(&mainQueues[qIndex].data[dataIndex].packet.data, packetData, currLength);
        mainQueues[qIndex].data[dataIndex].packet.order = orderForFlow[currFlow - offset];
        mainQueues[qIndex].data[dataIndex].packet.flow = currFlow;

        //Say that the spot is ready to be read
        mainQueues[qIndex].data[dataIndex].isOccupied = OCCUPIED;

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;

        //Update the next spot to be written in the queue
        mainQueues[qIndex].toWrite++;
        if(mainQueues[qIndex].toWrite >= BUFFERSIZE)
            mainQueues[qIndex].toWrite = 0;
        //mainQueues[qIndex].toWrite = mainQueues[qIndex].toWrite % BUFFERSIZE;
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
        
    //Decide which queues this output thread should manage
    size_t baseQueueIndex = threadNum * inputThreadCount;
    size_t maxQueueIndex = baseQueueIndex + inputThreadCount;

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_QUEUE] = {0}; 
    size_t qIndex = baseQueueIndex;
    size_t dataIndex = 0;

    //Say this thread is ready to process
    output[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(endFlag == 0){
        dataIndex = mainQueues[qIndex].toRead;

        //If there is no packet move to the next queue it is managing and start reading
        if(mainQueues[qIndex].data[dataIndex].isOccupied == NOT_OCCUPIED){
            qIndex++;
            if(qIndex >= maxQueueIndex) 
                qIndex = baseQueueIndex;
            continue;
        }

        //Get the current flow for the packet
        size_t currFlow = mainQueues[qIndex].data[dataIndex].packet.flow;
		
        //Packets order must be equal to the expected order.
        //Implementing less than currflow causes race conditions with writing
        //Any line that starts with a * is ignored by python script
        if(expected[currFlow] != mainQueues[qIndex].data[dataIndex].packet.order){
            /*Print out the contents of the processing queue that caused an error
            for(int i = 0; i < BUFFERSIZE; i++){
                printf("Position: %d, Flow: %ld, Order: %ld\n", i, mainQueues[qIndex].data[i].packet.flow, mainQueues[qIndex].data[i].packet.order);
            }
            */
            
            //Print out the specific packet that caused the error to the user
            printf("\nError Packet: Flow %lu | Order %lu\n", mainQueues[qIndex].data[dataIndex].packet.flow,mainQueues[qIndex].data[dataIndex].packet.order);
            printf("Packet out of order in Output Queue %lu. Expected %lu | Got %lu\n", threadNum, expected[currFlow], mainQueues[qIndex].data[dataIndex].packet.order);
            exit(0);
        }       
        //increment the number of packets passed
        output[threadNum].count++;

        //Set what the next expected packet for the flow should be
        expected[currFlow]++;

        //Set the position to free. Say it has already processed data
        mainQueues[qIndex].data[dataIndex].isOccupied = NOT_OCCUPIED;

        //Move to the next spot in the outputQueue to process
        mainQueues[qIndex].toRead++;
        if(mainQueues[qIndex].toRead >= BUFFERSIZE)
            mainQueues[qIndex].toRead = 0;
        //mainQueues[qIndex].toRead = mainQueues[qIndex].toRead % BUFFERSIZE;
    }

    return NULL;
}

void init_queues(){
    //initialize all values for built in input/output queues to 0
    for(int qIndex = 0; qIndex < MAX_NUM_INPUT_THREADS * MAX_NUM_OUTPUT_THREADS; qIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            mainQueues[qIndex].data[dataIndex].packet.flow = 0;
            mainQueues[qIndex].data[dataIndex].packet.order = 0;
            mainQueues[qIndex].data[dataIndex].packet.length = 0;
            mainQueues[qIndex].data[dataIndex].isOccupied = NOT_OCCUPIED;
        }
        mainQueues[qIndex].toRead = 0;
        mainQueues[qIndex].toWrite = 0;
    }
}

pthread_t * run(void *argsv){
    init_queues();
    return NULL;
}