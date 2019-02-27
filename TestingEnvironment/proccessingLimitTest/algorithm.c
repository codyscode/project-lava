/*
-This algorithm creates a single "wire".
-The wire work by grabbing the same amount of packets from each input queues then empties them into the appropriate output queue and it repeats this indefinitely
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

pthread_t passThreadIDS[MAX_NUM_OUTPUT_QUEUES];

void* testProcessing(void* args){
    //Assign threads to queues
    int* qIndex = (int*)(args);
    int order = 0;
    int flow = (*qIndex) + 1;
    int outputWriteIndex = 0;

    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        FENCE()
        //If the queue is full, then wait for it to become unfull
        while(output.queues[*qIndex].data[outputWriteIndex].flow != 0){
            ;
        }
        
        //Grab the packets data and move it into the output queue
        output.queues[*qIndex].data[outputWriteIndex].order = order;
        output.queues[*qIndex].data[outputWriteIndex].flow = flow;

        //Indicate the next spot to write to in the output queue
        if(outputWriteIndex == BUFFERSIZE - 1){
            outputWriteIndex = 0;
        }
        else{
            outputWriteIndex++;
        }

        order++;

        //This is a bandaid fix and makes this file useless. 
        //For some reason segfaults occur without this
        usleep(1);

        FENCE()
    }
}

void* determineSpeed(void* args){
    size_t currTotal = 0;
    size_t prevTotal = currTotal;
    size_t toPrint = 0;
    while(1){
        FENCE()
        //Every 10 Second Sample number of packets
        usleep(1000000);
        for(int i = 0; i < output.queueCount; i++){
            currTotal = output.queues[i].count;
        }
        toPrint = (currTotal - prevTotal);
        printf("Processing %lu packets per second\n", toPrint);
        fflush(NULL);
        prevTotal = currTotal;
        FENCE()
    }
}

void *run(void *argsv){
    printf("Metric is currently crippled because this is running \"too fast\", so a usleep was inserted as a bandaid. Any tips for fixing this would be helpful \n");
    fflush(NULL);

    //The output queue to write to
    int qOut[output.queueCount];

    for(int i = 0; i < output.queueCount; i++){
        qOut[i] = i;
    }

    for(int i = 0; i < output.queueCount; i++){
        //Create the thread
        Pthread_create(&passThreadIDS[i], NULL, testProcessing, (void *)&qOut[i]);
    }
    usleep(20000);
    Pthread_create(&passThreadIDS[8], NULL, determineSpeed, NULL);
}