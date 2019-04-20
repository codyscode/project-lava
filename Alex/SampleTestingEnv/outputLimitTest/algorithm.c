/*
-The idea behind this file is to test how fast a single output queue can process data in its queues. 
-Tests throughput of processing queues only
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

#define ALGNAME "processLimit"

pthread_t algThreadIDS[2];

void* testProcessing(void* args){
    //Assign thread to test core
    int queueNum = *((int *)args);
    int testCore = inputQueueCount + outputQueueCount + 2 + queueNum;

    //Set the thread to its own core
    set_thread_props(testCore);

    //Assign threads to queues
    int qIndex = queueNum;
    int order = 0;
    int flow = 1;
    int outputWriteIndex = 0;

    //Continuously push packets to the output queues
    while(1){
        //If the queue is full, then wait for it to become unfull
        while(output[qIndex].queue.data[outputWriteIndex].isOccupied == OCCUPIED){
            ;
        }
        
        //Grab the packets data and move it into the output queue
        output[qIndex].queue.data[outputWriteIndex].packet.order = order;
        output[qIndex].queue.data[outputWriteIndex].packet.flow = flow;

        //Indicate there is data to read
        output[qIndex].queue.data[outputWriteIndex].isOccupied = OCCUPIED;

        //Indicate the next spot to write to in the output queue
        if(outputWriteIndex == BUFFERSIZE - 1){
            outputWriteIndex = 0;
        }
        else{
            outputWriteIndex++;
        }

        order++;
    }
}

void* determineSpeedOutput(void* args){
    //Used for formatting numbers with commas
    setlocale(LC_NUMERIC, "");

    //Set the thread to its own core
    int spdCore = inputQueueCount + outputQueueCount + 1;
    set_thread_props(spdCore);
    
    int run = 0;

    size_t currTotal[8] = {0};
    size_t prevTotal[8] = {0};

    //Sample 10 times
    while(run < 10){
        //Every 1 Second Sample number of packets processed
        usleep(1000000);

        //Sample number of packets generated for all input queues
        for(int qIndex = 0; qIndex < outputQueueCount; qIndex++){
            currTotal[qIndex] = output[qIndex].queue.count;

            printf("Processing %'lu packets per second in queue %d\n", currTotal[qIndex] - prevTotal[qIndex], qIndex);

            prevTotal[qIndex] = currTotal[qIndex];
        }
        
        printf("\n");
        run++;
    }
    return NULL;
}

void *run(void *argsv){
    printf("\nStarting Process Limit Test...\n");
    fflush(NULL);

    int queueNum[MAX_NUM_INPUT_QUEUES];
    
    //Create the passer threads
    for(int i = 0; i < outputQueueCount; i++){
        queueNum[i] = i;
        Pthread_create(&algThreadIDS[0], NULL, testProcessing, &queueNum[i]);

        //detach passer thread
        Pthread_detach(algThreadIDS[0]);
    }

    Pthread_create(&algThreadIDS[1], NULL, determineSpeedOutput, NULL);

    Pthread_join(algThreadIDS[1], NULL);

    return NULL;
}

char* getName(){
    return ALGNAME;
}