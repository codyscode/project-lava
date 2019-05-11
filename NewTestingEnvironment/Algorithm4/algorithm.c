/*
This algorithm uses a single set of queues rather than input queues and output queues. 
The number of queues matches the number of input threads, i.e. each input thread writes to it's
own queue. The merging happends on the output side where each output thread figures out
which queues(s) it needs to read from. This is a simple algorithm where entire queues are
pidgeon-holed into output threads rather than individual flows being pidgeon-holed
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "singleSetPigeon"

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
// -Each input queue generates packets in a circular buffer
// -Input queue stops writing when the buffer is full and continues when it is empty
// -Each input queue generates 5 flows (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// -The flows can be scrambled coming from a single input (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
void * input_thread(void * args){
//Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    size_t threadNum = inputArgs->threadNum;   
    queue_t * inputQueue = &input[inputArgs->threadNum].queue;

    
    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Each input buffer has 5 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    size_t index = 0;
    
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    input[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //After having the base queue fill up, maintain all the packets in the queue by just overwriting their order number
    while(1){
        // *** START PACKET GENERATOR ***
        //Min value: offset || Max value: offset + 7
        seed0 = (214013 * seed0 + 2531011);   
        currFlow = ((seed0 >> 16) & FLOWS_PER_THREAD_MOD) + offset;

        //Min value: 64 || Max value: 8191 + 64
        seed1 = (214013 * seed1 + 2531011); 
        currLength = ((seed1 >> 16) % (MAX_PAYLOAD_SIZE_MOD - MIN_PAYLOAD_SIZE)) + MIN_PAYLOAD_SIZE; 
        // *** END PACKET GENERATOR  ***

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while((*inputQueue).data[index].isOccupied == OCCUPIED){
            ;//Do Nothing until a space is available to write
        }

        packet_t packet;
        packet.order = orderForFlow[currFlow - offset];
        packet.length = currLength;
        packet.flow = currFlow;
/*
        //Create the new packet. Write the order and length first before flow as flow > 0 indicates theres a packet there
        (*inputQueue).data[index].packet.order = orderForFlow[currFlow - offset];
        (*inputQueue).data[index].packet.length = currLength;
        (*inputQueue).data[index].packet.flow = currFlow;
*/        
        //memcpy simulates the packets data actually being written into the queue by the input thread
        memcpy(&(*inputQueue).data[index].packet, &packet, currLength + 24);

        //Update the next flow number to assign
        orderForFlow[currFlow - offset]++;

        //Say that the spot is ready to be read
        (*inputQueue).data[index].isOccupied = OCCUPIED;

        //Update the next spot to be written in the queue
        index++;
        index = index % BUFFERSIZE;
    }
    return NULL;
}

//This is the old output processing thread from the framework
//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order
void * output_thread(void * args){
    //Get arguments and any other functions for input threads
    threadArgs_t *outputArgs = (threadArgs_t *)args;

    size_t threadNum = outputArgs->threadNum;
    size_t currentQueue = threadNum;   
    queue_t *outputQueue = &(input[currentQueue].queue);

    //A dummy destination to copy the packet data to, simulating it being processed from the queue
    unsigned char dummyDestination[MAX_PAYLOAD_SIZE];

    //Set the thread to its own core
    set_thread_props(outputArgs->coreNum, 2);

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 
    size_t index; 

    output[currentQueue].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(1){
        index = (*outputQueue).toRead;
/*
        //If there is no packet, continuously check until something shows up
        while((*outputQueue).data[index].isOccupied == NOT_OCCUPIED){
            ;//Do Nothing until there is a packet to read
        }
*/
        if ((*outputQueue).data[index].isOccupied == NOT_OCCUPIED) {
            //Move to the next queue this output thread is responsible for
            currentQueue = currentQueue + outputThreadCount;
            if(currentQueue >= inputThreadCount) {
                currentQueue = threadNum;
            }
            outputQueue = &(input[currentQueue].queue);
            continue;
        }
        //Get the current flow for the packet
        size_t currFlow = (*outputQueue).data[index].packet.flow;

        // set expected order for given flow to the first packet that it sees
        if(expected[currFlow] == 0){
            expected[currFlow] = (*outputQueue).data[index].packet.order;
        }
		
        //Packets order must be equal to the expected order.
        //Implementing less than currflow causes race conditions with writing
        //Any line that starts with a * is ignored by python script
        if(expected[currFlow] != (*outputQueue).data[index].packet.order){
            //Print out the contents of the processing queue that caused an error
            for(int i = 0; i < BUFFERSIZE; i++){
                printf("Position: %d, Flow: %ld, Order: %ld\n", i, (*outputQueue).data[i].packet.flow, (*outputQueue).data[i].packet.order);
            }
            
            //Print out the specific packet that caused the error to the user
            printf("Error Packet: Flow %lu | Order %lu\n", (*outputQueue).data[index].packet.flow, (*outputQueue).data[index].packet.order);
            printf("Packet out of order in Output Queue %lu. Expected %lu | Got %lu\n", threadNum, expected[currFlow], (*outputQueue).data[index].packet.order);
            exit(0);
        }
        //Else "process" the packet by filling in 0 and incrementing the next expected
        else{            
            
            //Set what the next expected packet for the flow should be
            expected[currFlow]++;

            //Move to the next spot in the outputQueue to process
            (*outputQueue).toRead++;
            (*outputQueue).toRead = (*outputQueue).toRead % BUFFERSIZE;
	 
            //memcpy simulates the packets data being processed by the output thread.
	    memcpy(dummyDestination, (*outputQueue).data[index].packet.payload, (*outputQueue).data[index].packet.length);


            //increment the number of bits passed
            output[threadNum].byteCount += (*outputQueue).data[index].packet.length + 24;
            //Set the position to free. Say it has already processed data
            (*outputQueue).data[index].isOccupied = NOT_OCCUPIED;
/*
            //Move to the next queue this output thread is responsible for
            currentQueue = currentQueue + outputThreadCount;
            if(currentQueue >= inputThreadCount) {
                currentQueue = threadNum;
            }
            outputQueue = &(input[currentQueue].queue);
*/
        }
    }
    return NULL;
}


pthread_t * run(void *argsv){

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Tell the system we are setting the schedule for the thread, instead of inheriting
    if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
        perror("pthread_attr_setinheritsched");
    }
    return NULL;
}
