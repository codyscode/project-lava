/*
-The idea behind this file is to test how fast a single output queue can process data in its queues. 
-Tests throughput of processing queues only
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

#define ALGNAME "inputLimit"

pthread_t algThreadIDS[2];
size_t count;

void* testInputGeneration(void* args){
    //Assign threads to queues
    int qIndex = *((int *)args);

    //Assign thread to test core
    int testCore = input.queueCount + output.queueCount + 3 + qIndex;
    
    //Set the thread to its own core
    set_thread_props(testCore);

    int inputReadIndex = 0;
    size_t tempOrder;
    size_t tempFlow;

    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Wait for something to grab.
        while(input.queues[qIndex].data[inputReadIndex].flow == 0){
            ;
        }
        
        //simulate grabbing data
        tempOrder = input.queues[qIndex].data[inputReadIndex].order;
        tempFlow = input.queues[qIndex].data[inputReadIndex].flow;

        //mark the spot as free
        input.queues[qIndex].data[inputReadIndex].flow = 0;

        //Indicate the next spot to read from in the input queue
        inputReadIndex++;
        inputReadIndex = inputReadIndex % BUFFERSIZE;
    }
}

void* determineSpeedInput(void* args){
    int i = 0;

    //Set this thread to its own core
    int spdCore = input.queueCount + output.queueCount + 2;
    set_thread_props(spdCore);

    size_t currTotal[8] = {0};
    size_t prevTotal[8] = {0};

    //Sample 10 times
    while(i < 10){
        //Every 1 Second Sample number of packets grabbed
        usleep(1000000);

        //Sample number of packets generated for all input queues
        for(int qIndex = 0; qIndex < input.queueCount; qIndex++){
            currTotal[qIndex] = input.queues[qIndex].count;

            printf("Grabbing %lu packets per second from queue %d\n", currTotal[qIndex] - prevTotal[qIndex], qIndex);

            prevTotal[qIndex] = currTotal[qIndex];
        }

        printf("\n");

        i++;
    }
}

void *run(void *argsv){
    printf("Starting Metric\n");
    fflush(NULL);

    //set the core
    int runCore = input.queueCount + output.queueCount + 1;

    //queue num
    int queueNum[MAX_NUM_INPUT_QUEUES];

    //Set the thread to its own core
    set_thread_props(runCore);
    
    //Create the tester threads
    for(int i = 0; i < input.queueCount; i++){
        queueNum[i] = i;
        //Create the grabber threads
        Pthread_create(&algThreadIDS[i], NULL, testInputGeneration, (void *)&queueNum[i]);

        //detach grabber thread
        Pthread_detach(algThreadIDS[i]);
    }

    Pthread_create(&algThreadIDS[1], NULL, determineSpeedInput, NULL);

    //Wait for ten samples
    Pthread_join(algThreadIDS[1], NULL);
}

char* getName(){
    return ALGNAME;
}