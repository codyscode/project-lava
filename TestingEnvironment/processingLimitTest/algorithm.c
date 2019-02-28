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
    
    //Set the thread to its own core
    set_thread_props(testCore);

    //Assign threads to queues
    int qIndex = 0;
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

    //Set the thread to its own core
    set_thread_props(spdCore);

    size_t currTotal = 0;
    size_t prevTotal = currTotal;
    size_t toPrint = 0;
    while(1){
        //Every 1 Second Sample number of packets processed
        usleep(1000000);

        currTotal = output.queues[0].count;
        toPrint = (currTotal - prevTotal);

        printf("*Processing %lu packets per second\n", toPrint);
        fflush(NULL);

        prevTotal = currTotal;
    }
}

void *run(void *argsv){
    printf("*Starting Metric\n");
    fflush(NULL);

    //set the core
    int runCore = input.queueCount + output.queueCount + 1;

    //Set the thread to its own core
    set_thread_props(runCore);
    
    //Create the passer thread
    Pthread_create(&algThreadIDS[0], NULL, testProcessing, NULL);

    //Create the measuring thread
    Pthread_create(&algThreadIDS[1], NULL, determineSpeed, NULL);
}

char* getName(){
    return ALGNAME;
}