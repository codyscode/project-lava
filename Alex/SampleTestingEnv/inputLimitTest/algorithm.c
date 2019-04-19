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
    int testCore = inputQueueCount + outputQueueCount + 2 + qIndex;
    
    //Set the thread to its own core
    set_thread_props(testCore);

    int inputReadIndex = 0;

    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Wait for something to grab.
        while(input[qIndex].queue.data[inputReadIndex].isOccupied == NOT_OCCUPIED){
            ;
        }

        //We dont need to grab the data as we are seeing how fast we can make data
        //mark the spot as free
        input[qIndex].queue.data[inputReadIndex].isOccupied = NOT_OCCUPIED;

        //Indicate the next spot to read from in the input queue
        inputReadIndex++;
        inputReadIndex = inputReadIndex % BUFFERSIZE;
    }
}

void* determineSpeedInput(void* args){
    //Used for formatting numbers with commas
    setlocale(LC_NUMERIC, "");

    //Set this thread to its own core
    int spdCore = inputQueueCount + outputQueueCount + 1;
    set_thread_props(spdCore);
    
    int run = 0;

    size_t currTotal[8] = {0};
    size_t prevTotal[8] = {0};

    //Sample 10 times
    while(run < 10){
        //Every 1 Second Sample number of packets grabbed
        usleep(1000000);

        //Sample number of packets generated for all input queues
        for(int qIndex = 0; qIndex < inputQueueCount; qIndex++){
            currTotal[qIndex] = input[qIndex].queue.count;

            printf("Grabbing %'lu packets per second from queue %d\n", currTotal[qIndex] - prevTotal[qIndex], qIndex);

            prevTotal[qIndex] = currTotal[qIndex];
        }

        printf("\n");

        run++;
    }

    return NULL;
}

void *run(void *argsv){
    printf("\nStarting Input Limit Test...\n");
    fflush(NULL);

    //queue num
    int queueNum[MAX_NUM_INPUT_QUEUES];
    
    //Create the tester threads
    for(int i = 0; i < inputQueueCount; i++){
        queueNum[i] = i;
        //Create the grabber threads
        Pthread_create(&algThreadIDS[i], NULL, testInputGeneration, (void *)&queueNum[i]);

        //detach grabber thread
        Pthread_detach(algThreadIDS[i]);
    }

    Pthread_create(&algThreadIDS[1], NULL, determineSpeedInput, NULL);

    //Wait for ten samples
    Pthread_join(algThreadIDS[1], NULL);

    return NULL;
}

char* getName(){
    return ALGNAME;
}