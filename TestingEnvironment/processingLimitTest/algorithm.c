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
    int testCore = input.queueCount + output.queueCount + 2;
    int queueNum = *((int *)args);
    
    //Set the thread to its own core
    set_thread_props(testCore);

    //Assign threads to queues
    int qIndex = queueNum;
    int order = 0;
    int flow = 1;
    int outputWriteIndex = 0;

    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //If the queue is full, then wait for it to become unfull
        while(output.queues[qIndex].data[outputWriteIndex].flow != 0){
            ;
        }
        
        //Grab the packets data and move it into the output queue
        output.queues[qIndex].data[outputWriteIndex].order = order;
        output.queues[qIndex].data[outputWriteIndex].flow = flow;

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
    int spdCore = input.queueCount + output.queueCount + 3;
    int i = 0;
    int queueNum = *((int *)args);

    //Set the thread to its own core
    set_thread_props(spdCore);

    size_t currTotal = 0;
    size_t prevTotal = currTotal;
    size_t toPrint = 0;

    //Sample 10 times
    while(i < 10){
        //Every 1 Second Sample number of packets processed
        usleep(1000000);

        currTotal = output.queues[0].count;
        toPrint = (currTotal - prevTotal);

        printf("Processing %lu packets per second on queue %d\n", toPrint, queueNum);
        fflush(NULL);

        prevTotal = currTotal;
        i++;
    }
}

void *run(void *argsv){
    printf("Starting Metric\n");
    fflush(NULL);

    //set the core
    int runCore = input.queueCount + output.queueCount + 1;

    int queueNum[MAX_NUM_INPUT_QUEUES];

    //Set the thread to its own core
    set_thread_props(runCore);
    
    //Create the passer threads
    for(int i = 0; i < input.queueCount; i++){
        queueNum[i] = i;
        Pthread_create(&algThreadIDS[0], NULL, testProcessing, &queueNum[i]);

        //detach passer thread
        Pthread_detach(algThreadIDS[0]);
    }

    for(int i = 0; i < input.queueCount; i++){
        //Create the measuring thread
        Pthread_create(&algThreadIDS[1], NULL, determineSpeedOutput, &queueNum[i]);
    }


    for(int i = 0; i < input.queueCount; i++){
        //Wait for ten samples to finish
        Pthread_join(algThreadIDS[1], NULL);
    }
}

char* getName(){
    return ALGNAME;
}