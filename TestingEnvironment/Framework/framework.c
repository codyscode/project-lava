//REQUIRED:
// -algorithm.c that defines a function void* run(void*)
// -algorithm.h that defines the functions used in algorithm.c
// -algorithm.c needs to import global.h in order to have access to queues
// -alogrithm.c has access to clockspeed, which is an int that is a normalized value for the number of ticks in a second.
//  Use for measuring algorithm speed.

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
    queue_t * inputQueue = (queue_t *)inputArgs->queue;

    //Each input buffer has 5 flows associated with it that it generates
    int flowNum[FLOWS_PER_QUEUE] = {0};
    int currFlow, currLength;
    int offset = queueNum * FLOWS_PER_QUEUE;

    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    int index = 0;
    while(1){
        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = (rand() % FLOWS_PER_QUEUE) + offset + 1;
        currLength = (rand() % MAX_PACKET_SIZE) + MIN_PACKET_SIZE;

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while((*inputQueue).data[index].flow != 0){
            ;
        }

        //Create the new packet. Write the order and length first before flow as flow > 0 indicates theres a packet there
        (*inputQueue).data[index].order = flowNum[currFlow - offset - 1];
        (*inputQueue).data[index].length = currLength;
        (*inputQueue).data[index].flow = currFlow;

        //Update the next flow number to assign
        flowNum[currFlow - offset - 1]++;

        //Update the next spot to be written in the queue
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
    int numInputs = (int)input.queueCount;   
    queue_t *processQueue = (queue_t *)processArgs->queue;


    //"Proccess" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    int expected[numInputs * FLOWS_PER_QUEUE + 1]; 
    int index; 
    int threadCompleted = 0;

    //Initialize array to 0. Because it is dynamically allocated at runtime we need to use memset() or a for loop
    memset(expected, 0, (numInputs * FLOWS_PER_QUEUE + 1) * sizeof(int));

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
            
            for(int i = 0; i < BUFFERSIZE; i++){
                printf("Position: %d, Flow: %ld, Order: %ld\n", i, (*processQueue).data[i].flow, (*processQueue).data[i].order);
            }
            
            printf("Error Packet: Flow %lu | Order %lu\n", (*processQueue).data[index].flow, (*processQueue).data[index].order);
            printf("Packet out of order in Output Queue %d. Expected %d | Got %lu\n", queueNum, expected[currFlow], (*processQueue).data[index].order);
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
        
        //Once the queue has processed a certain amount of packets. Indicate the thread has completed the required amount of work.
        if((*processQueue).count >= RUNTIME && threadCompleted  == 0){
            printf("Successfully Processed %lu Packets in Output Queue %d\n", (*processQueue).count, queueNum + 1);
            output.finished[queueNum] = 1;
            threadCompleted = 1;
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
    input.queueCount = atoi(argv[1]);
    output.queueCount = atoi(argv[2]);

    //Make sure that the number of input and output queues is valid
    assert(input.queueCount <= MAX_NUM_INPUT_QUEUES);
    assert(output.queueCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(input.queueCount > 0 && output.queueCount > 0);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Get a baseline for the number of ticks in a second
    //clockSpeed = cal();

    //initialize input queues
    for(int queueIndex = 0; queueIndex < input.queueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            input.queues[queueIndex].data[dataIndex].flow = 0;
            input.queues[queueIndex].data[dataIndex].order = 0;
        }
        input.queues[queueIndex].toRead = 0;
        input.queues[queueIndex].toWrite = 0;
    }

    //initialize ouput queues
    for(int queueIndex = 0; queueIndex < output.queueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            output.queues[queueIndex].data[dataIndex].flow = 0;
            output.queues[queueIndex].data[dataIndex].order = 0;
        }
        output.queues[queueIndex].toRead = 0;
        output.queues[queueIndex].toWrite = 0;
    }


    int mutexErr;
    //Initialize input locks
    for(int index = 0; index < MAX_NUM_INPUT_QUEUES; index++){
        Pthread_mutex_init(&input.locks[index], NULL);
    }

    //Initialize output locks
    for(int index = 0; index < MAX_NUM_OUTPUT_QUEUES; index++){
        Pthread_mutex_init(&output.locks[index], NULL);
    }


    int pthreadErr;
    //create the input generation threads.
    //Each input queue has a thread associated with it
    for(int index = 0; index < input.queueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        input.threadArgs[index].queue = &input.queues[index];
        input.threadArgs[index].queueNum = index;
 
        //Make sure data is written
        FENCE()

        //Spawn input thread
        Pthread_create(&input.threadIDs[index], &attrs, input_thread, (void *)&input.threadArgs[index]);

        //Detach the thread
        Pthread_detach(input.threadIDs[index]);
    }

    //create the output processing threads.
    //Each output queue has a thread associated with it
    for(int index = 0; index < output.queueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        output.threadArgs[index].queue = &output.queues[index];
        output.threadArgs[index].queueNum = index;

        //Make sure data is written
        FENCE()

        //Spawn the thread
        Pthread_create(&output.threadIDs[index], &attrs, processing_thread, (void *)&output.threadArgs[index]);

        //Detach the thread
        Pthread_detach(output.threadIDs[index]);
    }


    //Print out testing information to the user
    printf("\nTesting with: \n%lu Input Queues \n%lu Output Queues\n\n", input.queueCount, output.queueCount);

    //Allow buffers to fill
    usleep(20000);

    printf("Starting passing\n\n");

    //The algorithm portion that gets inserted into the code
    //--Another option is to move the global variables/non input/output thread functions into a different header function <global.h>--
    //--and put that header in both the framework and algorithm code--
    //--Allows for better abstraction--

    //Run thread ID
    pthread_t RunID;

    //Spawn the thread that handles the algorithm portion.
    //We are allowed to spawn threads from threads, so there is no conflict here
    Pthread_create(&RunID, &attrs, run, NULL);

    //Wait for all processing queues to process x amount of packets before quiting.
    //We do it this way instead of pthread_join to allow the processing thread to continue running after finishing its job
    //Otherwise the output queue fills up and this can cause stalls on certain algorithms.
    //The thread only updates the array on finishing and uses local variables for the if statement in order to improve performance
    for(int i = 0; i < output.queueCount; i++){
        //While the thread is not finished
        while(output.finished[i] == 0){
            ;
        }
    };
    printf("\nFinished Processing. All Packets are in Correct Order\n");
} 