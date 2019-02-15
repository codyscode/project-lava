//REQUIRED:
// -algorithm.c that defines a function run(algoArgs*)
// -algorithm.h that defines the functions used in algorithm.c
// -algorithm.c needs to import global.h in order to have access to queues

#include"global.h"
#include"wrapper.h"
#include"algorithm.h"

//The job of the input threads is to make packets to populate the buffers. As of now the packets are stored in a buffer.
//--Need to work out working memory simulation for packet retrieval--
//Attributes:
// -Each input queue generates packets in a circular buffer
// -Input queue stops writing when the buffer is full and continues when it is empty
// -Each input queue generates 5 flows (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// -The flows can be scrambled coming from a single input (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
void* input_thread(void *args){
    //Get argurments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    int queueNum = inputArgs->queueNum;
    int queueCount = 0;
    queue_t * inputQueue = (queue_t *)inputArgs->queue;

    //Each input buffer has 5 flows associated with it that it generates
    int flowNum[FLOWS_PER_QUEUE] = {0};
    int currFlow;
    int offset = queueNum * FLOWS_PER_QUEUE;

    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will switch the other queue and start filling it
    int index = 0;
    while(1){
        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = (rand() % FLOWS_PER_QUEUE) + offset + 1;

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while((*inputQueue).data[index].flow != 0){
            ;
        }
        //Create the new packet
        (*inputQueue).data[index].flow = currFlow;
        (*inputQueue).data[index].order = flowNum[currFlow - offset - 1];

        //Update the next flow number to assign
        flowNum[currFlow - offset - 1]++;

        //Update the next spot to be written in the associated queue
        index++;
        index = index % BUFFERSIZE;
    }
}

//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order
//--Need to work out storing in memory for proper simulation--
void* processing_thread(void *args){
    //Get argurments and any other functions for input threads
    threadArgs_t *processArgs = (threadArgs_t *)args;

    int queueNum = processArgs->queueNum;
    int numInputs = (int)(processArgs->inputQueueCount);
    queue_t *processQueue = (queue_t *)processArgs->queue;


    //"Proccess" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    int expected[numInputs * FLOWS_PER_QUEUE + 1]; 
    int index; 

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(1){
        index = (*processQueue).toRead;
        //If there is no packet, contiuously check until something shows up
        while((*processQueue).data[index].flow == 0){
            ;
        }
        //Get the current flow for the packet
        int currFlow = (*processQueue).data[index].flow;

        //If a packet is grabbed out of order exit
        if(expected[currFlow] != (*processQueue).data[index].order){
            /*
            for(int i = 0; i < numInputs * FLOWS_PER_QUEUE + 1; i++){
                printf("Flow: %d, Expected: %d\n", i, expected[i]);
            }
            */
            printf("Error Packet: Flow %lu | Order %lu\n", (*processQueue).data[index].flow, (*processQueue).data[index].order);
            printf("Packet out of order in Processing Queue %d. Expected %d | Got %lu\n", queueNum - numInputs + 1, expected[currFlow], (*processQueue).data[index].order);
            exit(0);
        }
        //Else "process" the packet by filling in 0 and incrementing the next expected
        else{            
            //Increment how many packets the queue has processed
            (*processQueue).count++;

            //Set what the next expected packet for the flow should be
            expected[currFlow]++;

            //Set the position to free
            (*processQueue).data[index].flow = 0;

            //Move to the next spot in the outputQueue to process
            (*processQueue).toRead++;
            (*processQueue).toRead = (*processQueue).toRead % BUFFERSIZE;
        }
        
        if((*processQueue).count > RUNTIME){
            printf("Successfully Processed %lu Packets in Output Queue %d\n", (*processQueue).count, queueNum - numInputs + 1);
            exit(0);
        }
        
    }
}

void main(int argc, char**argv){
    //Error checking for proper running
    if (argc < 3){
        printf("Usage: Queues <# input queues>, <# output queues>\n");
        exit(0);
    }

    //Grab the number of input and output queues to use
    int inputQueueCount = atoi(argv[1]);
    int outputQueueCount = atoi(argv[2]);
    int totalQueueCount = inputQueueCount + outputQueueCount;

    //Make sure that the number of input and output queues is valid
    assert(inputQueueCount <= MAX_NUM_INPUT_QUEUES);
    assert(outputQueueCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(inputQueueCount > 0 && outputQueueCount > 0);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Get a baseline for the number of ticks in a second
    //clockSpeed = cal();

    //initialize all queues
    for(int queueIndex = 0; queueIndex < inputQueueCount + outputQueueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            queues[queueIndex].data[dataIndex].flow = 0;
            queues[queueIndex].data[dataIndex].order = 0;
        }
        queues[queueIndex].toRead = 0;
        queues[queueIndex].toWrite = 0;
    }

    //Initialize all locks
    int mutexErr;
    for(int index = 0; index < MAX_NUM_QUEUES; index++){
        Pthread_mutex_init(&locks[index], NULL);
    }

    //create the input generation threads and output processing threads.
    //Each input an output queue has a thread associated with it
    int pthreadErr;
    for(int index = 0; index < totalQueueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        threadArgs[index].queue = &queues[index];
        threadArgs[index].queueNum = index;
        threadArgs[index].inputQueueCount = inputQueueCount;
        threadArgs[index].outputQueueCount = outputQueueCount;

        //Assign second queue and spawn input threads
        if(index < inputQueueCount){
            //Make sure data is written
            FENCE()

            //Spawn the thread
            Pthread_create(&threadIDs[index], &attrs, input_thread, (void *)&threadArgs[index]);
        }
        //Assign second queue and Spawn output threads
        else{
            //Make sure data is written
            FENCE()

            //Spawn the thread
            Pthread_create(&threadIDs[index], &attrs, processing_thread, (void *)&threadArgs[index]);
        }
    }
    //Print out testing information to the user
    printf("\nTesting with: \n%d Input Queues \n%d Output Queues\n\n", inputQueueCount, outputQueueCount);

    //Allow buffers to fill
    usleep(1000000);

    printf("Starting passing\n\n");

    //The algorithm portion that gets inserted into the code
    //--Another option is to move the global variables/non input/output thread functions into a different header function <global.h>--
    //--and put that header in both the framework and algorithm code--
    //--Allows for better abstraction--

    //The run function takes as arguments:
    // -the number of input queues
    // -the number of output queues
    algoArgs_t algoArgs;
    algoArgs.inputQueueCount = inputQueueCount;
    algoArgs.outputQueueCount = outputQueueCount;
    run(&algoArgs);
    printf("Done\n");
} 