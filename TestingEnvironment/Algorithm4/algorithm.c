/*
This algorithm uses a single set of queues rather than input queues and output queues. 
The number of queues matches the number of input threads, i.e. each input thread writes to it's
own queue. The merging happends on the output side where each output thread figures out
which queues(s) it needs to read from. This is a simple algorithm where entire queues are
pidgeon-holed into output threads rather than individual flows being pidgeon-holed
*/

#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

#define ALGNAME "Algorithm4"

char* get_name(){
    return ALGNAME;
}

function get_input_thread(){
    return input_thread;
}

function get_output_thread(){
    return output_thread;
}

void * input_thread(void * args){
    //Get arguments for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    size_t threadNum = inputArgs->threadNum;   
    queue_t * inputQueue = &input[inputArgs->threadNum].queue;
    
    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum, 2);

    //Each input buffer has 5 flows associated with it that it generates
    size_t orderForFlow[FLOWS_PER_THREAD] = {0};
    size_t currFlow, currLength;
    size_t offset = threadNum * FLOWS_PER_THREAD;
	
    size_t index = 0;
    
    register unsigned int seed0 = (unsigned int)time(NULL);
    register unsigned int seed1 = (unsigned int)time(NULL);

    input[threadNum].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

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
            ;
        }

        packet_t packet;
        packet.order = orderForFlow[currFlow - offset];
        packet.length = currLength;
        packet.flow = currFlow;
   
        //memcpy simulates the packets data actually being written into the queue by the input thread
        memcpy(&(*inputQueue).data[index].packet, &packet, currLength + PACKET_HEADER_SIZE);

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

    //Verifies order for a given flow
    size_t expected[MAX_NUM_INPUT_THREADS * FLOWS_PER_THREAD] = {0}; 
    size_t index; 

    output[currentQueue].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);

    while(1){
        index = (*outputQueue).toRead;

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
        else{            
            
            //Set what the next expected packet for the flow should be
            expected[currFlow]++;

            //Move to the next spot in the outputQueue to process
            (*outputQueue).toRead++;
            (*outputQueue).toRead = (*outputQueue).toRead % BUFFERSIZE;
	 
            //memcpy simulates the packets data being processed by the output thread.
	        memcpy(dummyDestination, (*outputQueue).data[index].packet.payload, (*outputQueue).data[index].packet.length);

            //increment the number of bits passed
            output[threadNum].byteCount += (*outputQueue).data[index].packet.length + PACKET_HEADER_SIZE;
            //Set the position to free. Say it has already processed data
            (*outputQueue).data[index].isOccupied = NOT_OCCUPIED;
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
