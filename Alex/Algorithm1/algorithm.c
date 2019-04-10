/*
-This algorithm creates a single "wire".
-The wire works by grabbing the same amount of packets from each input queues then empties them into the appropriate output queue and it repeats this indefinitely
-The algorithm uses 3 sets of queues, One input, One wire, and One Output queue
*/

#include "../Framework/global.h"
#include "../Framework/wrapper.h"

#define ALGNAME "singleWire"

typedef struct WorkerThreadArgs{
    int toGrabCount;
    queue_t * mainQueue;
}workerThreadArgs_t;

pthread_t worker;
workerThreadArgs_t wThreadArgs;

char* getName(){
    return ALGNAME;
}

function get_fill_method(){
    return NULL;
}

function get_drain_method(){
    return NULL;
}

void spawn_input_process(pthread_attr_t attrs, function input_function{
    //Core for each input thread to be assigned to
    int core = 1;

    //Each input queue has a thread associated with it
    for(int index = 0; index < inputQueueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        input[index].threadArgs.queue = &input[index].queue;
        input[index].threadArgs.queueNum = index;
        input[index].threadArgs.coreNum = core;
        core++;

        //Spawn input thread
        Pthread_create(&input[index].threadID, &attrs, input_function, (void *)&input[index].threadArgs);

        //Detach the thread
        Pthread_detach(input[index].threadID);
    }
}

void spawn_output_process(pthread_attr_t attrs, function output_function){
    //Core for each output thread to be assigned to
    //Next available queue after cores have been assigned ot input queues
    int core = inputQueueCount + 1;

    //Each output queue has a thread associated with it
    for(int index = 0; index < outputQueueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        output[index].threadArgs.queue = &output[index].queue;
        output[index].threadArgs.queueNum = index;
        output[index].threadArgs.coreNum = core;
        core++;

        //Spawn the thread
        Pthread_create(&output[index].threadID, &attrs, processing_thread, (void *)&output[index].threadArgs);

        //Detach the thread
        Pthread_detach(output[index].threadID);
    }
}

void grabPackets(int toGrabCount, queue_t* mainQueue){
    //Go through each queue and grab the stated amount of packets from it
    for(int qIndex = 0; qIndex < inputQueueCount; qIndex++){
        for(int packetCount = 0; packetCount < toGrabCount; packetCount++){
            //Used for readability
            int mainWriteIndex = (*mainQueue).toWrite;
            int inputReadIndex = input[qIndex].queue.toRead;

            //If there is no where to write then start processing
            if((*mainQueue).data[mainWriteIndex].isOccupied == OCCUPIED){
                qIndex = inputQueueCount;
                break;
            }

            //If there is nothing to grab then move to next queue
            if(input[qIndex].queue.data[inputReadIndex].isOccupied == NOT_OCCUPIED){
                break;
            }

            //Grab the packets data and move it into the mainQueue
            (*mainQueue).data[mainWriteIndex].packet.flow = input[qIndex].queue.data[inputReadIndex].packet.flow;
            (*mainQueue).data[mainWriteIndex].packet.order = input[qIndex].queue.data[inputReadIndex].packet.order;

            //Indicate the next spot to read from in the input queue
            input[qIndex].queue.toRead++;
            input[qIndex].queue.toRead = input[qIndex].queue.toRead % BUFFERSIZE;

            //Indicade the next spot to write to in the main queue
            (*mainQueue).toWrite++;
            (*mainQueue).toWrite = (*mainQueue).toWrite % BUFFERSIZE;

            //Indicate the space is free to write to in the input queue
            input[qIndex].queue.data[inputReadIndex].isOccupied = NOT_OCCUPIED;

            //Indicate the space has data in the main queue
            (*mainQueue).data[inputReadIndex].isOccupied = OCCUPIED;
        }
    }
}

void passPackets(queue_t* mainQueue){
    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Used for readability
        int mainReadIndex = (*mainQueue).toRead;

        //If there is no more data, then exit and get more packets
        if((*mainQueue).data[mainReadIndex].isOccupied == NOT_OCCUPIED){
            break;
        }

        //Get the appropriate output queue that this should be sending to
        int qIndex = (*mainQueue).data[mainReadIndex].packet.flow % outputQueueCount;

        //Used for readability
        int outputWriteIndex = output[qIndex].queue.toWrite;

        //If the queue is full, then wait for it to become unfull
        while(output[qIndex].queue.data[outputWriteIndex].isOccupied == OCCUPIED){
            ;
        }

        //Grab the packets data and move it into the output queue
        output[qIndex].queue.data[outputWriteIndex].packet.order = (*mainQueue).data[mainReadIndex].packet.order;
        output[qIndex].queue.data[outputWriteIndex].packet.flow = (*mainQueue).data[mainReadIndex].packet.flow;

        //Indicate the next spot to write to in the output queue
        output[qIndex].queue.toWrite++;
        output[qIndex].queue.toWrite = output[qIndex].queue.toWrite % BUFFERSIZE;

        //Indicade the next spot to read from in the main queue
        (*mainQueue).toRead++;
        (*mainQueue).toRead = (*mainQueue).toRead % BUFFERSIZE;

        //Indicate the space is free to write to in the main queue
        (*mainQueue).data[mainReadIndex].isOccupied = NOT_OCCUPIED;

        //Indicate the space is ready to read from in the output queue
        output[qIndex].queue.data[outputWriteIndex].isOccupied = OCCUPIED;
    }
}

void * workerThread(void* args){
    //Get argument struct into local variables
    workerThreadArgs_t wargs = *((workerThreadArgs_t *)args);
    int toGrabCount = (int)(wargs.toGrabCount);
    queue_t * mainQueue = (queue_t *)(wargs.mainQueue);

    //Set the thread to its own core
    //+1 is neccessary as we have core 0, input threads, output threads
    set_thread_props(inputQueueCount + outputQueueCount + 1);

    //Allow the input queues to fill
    fill_queues()

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
    int toGrabCount = BUFFERSIZE / inputQueueCount;
    
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