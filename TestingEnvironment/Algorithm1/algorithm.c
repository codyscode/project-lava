/*
-This algorithm creates a single "wire".
-The wire work by grabbing the same amount of packets from each input queues then empties them into the appropriate output queue and it repeats this indefinitely
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

#define ALGNAME "singleWire"

typedef struct WorkerThreadArgs{
    int toGrabCount;
    queue_t * mainQueue;
}workerThreadArgs_t;

pthread_t worker;
workerThreadArgs_t wThreadArgs;

void grabPackets(int toGrabCount, queue_t* mainQueue){
    //Go through each queue and grab the stated amount of packets from it
    for(int qIndex = 0; qIndex < input.queueCount; qIndex++){
        for(int packetCount = 0; packetCount < toGrabCount; packetCount++){
            //Used for readability
            int mainWriteIndex = (*mainQueue).toWrite;
            int inputReadIndex = input.queues[qIndex].toRead;

            //If there is no where to write then start processing
            if((*mainQueue).data[mainWriteIndex].flow != 0){
                qIndex = input.queueCount;
                break;
            }

            //If there is nothing to grab then move to next queue
            if(input.queues[qIndex].data[inputReadIndex].flow == 0){
                break;
            }

            //Grab the packets data and move it into the mainQueue
            (*mainQueue).data[mainWriteIndex].flow = input.queues[qIndex].data[inputReadIndex].flow;
            (*mainQueue).data[mainWriteIndex].order = input.queues[qIndex].data[inputReadIndex].order;

            //Indicate the next spot to read from in the input queue
            input.queues[qIndex].toRead++;
            input.queues[qIndex].toRead = input.queues[qIndex].toRead % BUFFERSIZE;

            //Indicade the next spot to write to in the main queue
            (*mainQueue).toWrite++;
            (*mainQueue).toWrite = (*mainQueue).toWrite % BUFFERSIZE;

            //Ensure the flow is updated last
            FENCE()

            //Indicate the space is free to write to in the input queue
            input.queues[qIndex].data[inputReadIndex].flow = 0;
        }
    }
}

void passPackets(queue_t* mainQueue){
    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Used for readability
        int mainReadIndex = (*mainQueue).toRead;

        //If there is no more data, then exit and get more packets
        if((*mainQueue).data[mainReadIndex].flow == 0){
            break;
        }

        //Get the appropriate output queue that this should be sending to
        int qIndex = (*mainQueue).data[mainReadIndex].flow % output.queueCount;

        //Used for readability
        int outputWriteIndex = output.queues[qIndex].toWrite;

        //If the queue is full, then wait for it to become unfull
        while(output.queues[qIndex].data[outputWriteIndex].flow != 0){
            ;
        }

        //Grab the packets data and move it into the output queue
        output.queues[qIndex].data[outputWriteIndex].order = (*mainQueue).data[mainReadIndex].order;
        output.queues[qIndex].data[outputWriteIndex].flow = (*mainQueue).data[mainReadIndex].flow;

        //Indicate the next spot to write to in the output queue
        output.queues[qIndex].toWrite++;
        output.queues[qIndex].toWrite = output.queues[qIndex].toWrite % BUFFERSIZE;

        //Indicade the next spot to read from in the main queue
        (*mainQueue).toRead++;
        (*mainQueue).toRead = (*mainQueue).toRead % BUFFERSIZE;

        //Update that the positon is free, last
        FENCE()

        //Indicate the space is free to write to in the main queue
        (*mainQueue).data[mainReadIndex].flow = 0;
    }
}

void * workerThread(void* args){
    //Get argument struct into local variables
    workerThreadArgs_t wargs = *((workerThreadArgs_t *)args);
    int toGrabCount = (int)(wargs.toGrabCount);
    queue_t * mainQueue = (queue_t *)(wargs.mainQueue);

    //Set the thread to its own core
    //+1 is neccessary as we have core 0, input threads, output threads
    set_thread_props(input.queueCount + output.queueCount + 1);

    //start the alarm
    alarm_start();

    //wait for the alarm to start
    while(startFlag == 0);

    while(endFlag == 0){
        grabPackets(toGrabCount, mainQueue);
        passPackets(mainQueue);
    }

    free(mainQueue);

    return NULL;
}

void *run(void *argsv){
    //The amount of packets to grab from each queue. This algorithm gives all queues equal priority
    int toGrabCount = BUFFERSIZE / input.queueCount;

    //The main "wire" queue where everything will be written
    queue_t *mainQueue = Malloc(sizeof(queue_t));
    mainQueue->toRead = 0;
    mainQueue->toWrite = 0;

    //set the alarm
    alarm_init();

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    wThreadArgs.mainQueue = mainQueue;
    wThreadArgs.toGrabCount = toGrabCount;

    //Tell the system we are setting the schedule for the thread, instead of inheriting
    if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
        perror("pthread_attr_setinheritsched");
    }

    //Spawn the worker thread
    Pthread_create(&worker, &attrs, workerThread, (void *)&wThreadArgs);

    //Wait for the thread to finish
    Pthread_join(worker, NULL);

    return NULL;
}

char* getName(){
    return ALGNAME;
}