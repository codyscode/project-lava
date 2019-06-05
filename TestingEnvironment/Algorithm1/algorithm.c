/*
Created By: Alex Widmann
Last Modified: 3 June 2019

-   This algorithm makes M x N queues (buffers) communicate data. This 
-   allows all input threads to communicate with all output threads.
-   It uses one set of queues that the input threads write to directly 
-   and the output threads read from. Packets are not grouped into vectors
-   and as soon as it is placed in the buffer and the output thread is 
-   ready to read it, the packet is processed
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm1"

queue_t mainQueues[MAX_NUM_INPUT_THREADS * MAX_NUM_OUTPUT_THREADS];

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
-   corresponding queue based on the flow which is modded with the number
-   of flows a thread is instructed to generate (defined in global.h). 
-   When the buffer the input thread is attempting to write to is full
-   it sits on a spin lock until the buffer is free to write to again. Due
-   to testing for absolute performance we used spinlocks to avoid context
-   switches as much as possible. In a real world scenario this would be
-   using semaphores or other sleep based locking methods.

-   Each input thead generates x flows:
-   (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
-   The flows can be scrambled coming from a single input:
-   (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)

-   Packets are hashed to the appropriate queue using the modulus operator
-   and the number of queues said input thread has sole writing access to.
*/
void * input_thread(void * args){
    //Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = inputArgs->threadNum;   
    set_thread_props(inputArgs->coreNum, 2);

    //Dummy Data to write to the packet
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //Queues that the input thread writes to
    //Uses a base and limit approach for indexing in its queues
    size_t baseQueueIndex = threadNum * outputThreadCount;
    size_t maxQueueIndex = baseQueueIndex + outputThreadCount;

    //Each input buffer has 5 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    //Index for the corresponding buffer and the index within the buffer
    //to write to
    size_t dataIndex = 0;
    size_t qIndex = 0;

    //Used to randomly generate packets and their headers
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    //Say this thread is ready to generate and pass
    input[threadNum].readyFlag = 1;

    //Wait until everything else is ready. Framework signals start
    while(startFlag == 0);

    //Write packets to their corresponding queues
    while(1){
        // *** START PACKET GENERATOR ***
        //Min value: offset || Max value: offset + 7
        seed0 = (214013 * seed0 + 2531011);   
        currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

        //Min value: 64 || Max value: 8191 + 64
        seed1 = (214013 * seed1 + 2531011); 
        currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
        // *** END PACKET GENERATOR  ***

        //Determine which queue to write the packet data to
        qIndex = (currFlow % (maxQueueIndex - baseQueueIndex)) + baseQueueIndex;
        dataIndex = mainQueues[qIndex].toWrite;

        //If the queue spot is filled then that means the input buffer is 
        //full so continuously check until it becomes open
        while(mainQueues[qIndex].data[dataIndex].isOccupied == OCCUPIED){
            ;//Do Nothing until a space is available to write
        }

        //Write the packet data to the queue
        memcpy(&mainQueues[qIndex].data[dataIndex].packet.payload, packetData, currLength);
        mainQueues[qIndex].data[dataIndex].packet.order = orderForFlow[currFlow - offset];
        mainQueues[qIndex].data[dataIndex].packet.flow = currFlow;
        mainQueues[qIndex].data[dataIndex].packet.length = currLength;

        //Say that the spot is ready to be read
        mainQueues[qIndex].data[dataIndex].isOccupied = OCCUPIED;

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;

        //Update the next spot to be written in the queue
        mainQueues[qIndex].toWrite++;
        if(mainQueues[qIndex].toWrite >= BUFFERSIZE) 
            mainQueues[qIndex].toWrite = 0;
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
    //Get arguments and any other functions for output threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    //Set the thread to its own core
    size_t threadNum = outputArgs->threadNum;   
    set_thread_props(outputArgs->coreNum, 2);
        
    //Decide which queues this output thread should manage
    //Uses base and limit approach
    size_t baseQueueIndex = threadNum;
    size_t maxQueues = inputThreadCount * outputThreadCount;

    //used to "process" packets to confirm they are in the correct order 
    //before consuming more. Processing threads process until they get 
    //to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 
    size_t qIndex = baseQueueIndex;
    size_t dataIndex = 0;

    //Packet data
    unsigned char packetData[MAX_PAYLOAD_SIZE];

    //Say this thread is ready to process
    output[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Go through each space in the output queue until we reach an emtpy 
    //space in which case we swap to the other queue to process its packets
    while(1){
        dataIndex = mainQueues[qIndex].toRead;

        //If there is no packet move to the next queue it is managing and 
        //start reading
        if(mainQueues[qIndex].data[dataIndex].isOccupied == NOT_OCCUPIED){
            qIndex += outputThreadCount;
            if(qIndex >= maxQueues) 
                qIndex = baseQueueIndex;
            continue;
        }

        //Get the current flow for the packet
        size_t currFlow = mainQueues[qIndex].data[dataIndex].packet.flow;
		
        //Packets order must be equal to the expected order.
        if(expected[currFlow] != mainQueues[qIndex].data[dataIndex].packet.order){
            //Print out the contents of the processing queue that caused an error
            for(int i = 0; i < BUFFERSIZE; i++){
                printf("Position: %d, Flow: %ld, Order: %ld\n", i, mainQueues[qIndex].data[i].packet.flow, mainQueues[qIndex].data[i].packet.order);
            }
            
            //Print out the specific packet that caused the error to the user
            printf("\nError Packet: Flow %lu | Order %lu\n", mainQueues[qIndex].data[dataIndex].packet.flow,mainQueues[qIndex].data[dataIndex].packet.order);
            printf("Packet out of order in Output Queue %lu. Expected %lu | Got %lu\n", threadNum, expected[currFlow], mainQueues[qIndex].data[dataIndex].packet.order);
            exit(1);
        }    
        size_t currLength = mainQueues[qIndex].data[dataIndex].packet.length;

        //Pull the data out of the packet
        memcpy(packetData, &mainQueues[qIndex].data[dataIndex].packet.payload, currLength);

        //Set the position to free. Say it has already processed data
        mainQueues[qIndex].data[dataIndex].isOccupied = NOT_OCCUPIED;

        //increment the number of packets passed
        output[threadNum].byteCount += currLength + PACKET_HEADER_SIZE;

        //Set what the next expected packet for the flow should be
        expected[currFlow]++;

        //Move to the next spot in the outputQueue to process
        mainQueues[qIndex].toRead++;
        if(mainQueues[qIndex].toRead >= BUFFERSIZE) 
            mainQueues[qIndex].toRead = 0;
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
