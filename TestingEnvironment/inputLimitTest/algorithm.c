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
    //Assign thread to test core
    int testCore = input.queueCount + output.queueCount + 2;
    
    //Set the thread to its own core
    set_thread_props(testCore);

    //Assign threads to queues
    int qIndex = 0;
    int order = 0;
    int flow = 1;
    int inputReadIndex = 0;

    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Wait for something to grab.
        while(input.queues[qIndex].data[inputReadIndex].flow == 0){
            ;
        }
        
        //mark the spot as free
        //Dont need to grab data as we are seeing how fast input threads generate
        input.queues[qIndex].data[inputReadIndex].flow = 0;

        //Indicate the next spot to read from in the input queue
        if(inputReadIndex == BUFFERSIZE - 1){
            inputReadIndex = 0;
        }
        else{
            inputReadIndex++;
        }

        //Increment the number of packets grabbed
        count++;
    }
}

void* determineSpeedInput(void* args){
    int spdCore = input.queueCount + output.queueCount + 3;

    //Set the thread to its own core
    set_thread_props(spdCore);

    size_t currTotal = 0;
    size_t prevTotal = currTotal;
    size_t toPrint = 0;
    while(1){
        //Every 1 Second Sample number of packets grabbed
        usleep(1000000);

        currTotal = count;
        toPrint = (currTotal - prevTotal);

        printf("*Grabbing %lu packets per second\n", toPrint);
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
    
    //Create the grabber thread
    Pthread_create(&algThreadIDS[0], NULL, testInputGeneration, NULL);

    //Create the measuring thread
    Pthread_create(&algThreadIDS[1], NULL, determineSpeedInput, NULL);
}

char* getName(){
    return ALGNAME;
}