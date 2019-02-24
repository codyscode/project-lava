/* 
-This algorithm creates a number of "wires" equal to either input or output queues (the smaller count of the two).
-Input and output queues are partitioned to each queue as evenly as possible and each "wire" is responsible for
 transfering the data from its input to its output queues
-The wires work by grabbing packets from each of its input queues then empties them into the appropriate output queue and it repeats this indefinitely
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

typedef struct multiWireArgs{
    int baseInputQueueNum;
    int numInputQueuesAssigned;
    int baseOutputQueueNum;
    int numOutputQueuesAssigned;
}margs_t;

pthread_t moverThreadIds[MAX_NUM_INPUT_QUEUES];
margs_t algThreadArgs[MAX_NUM_INPUT_QUEUES];

void multiGrabPackets(int toGrabCount, int baseInputQueueIndex, int numInputQueues, queue_t* mainQueue){
    //Go through each queue and grab the stated amount of packets from it
    int maxQueueIndex = baseInputQueueIndex + numInputQueues;
    for(int qIndex = baseInputQueueIndex; qIndex <  maxQueueIndex; qIndex++){
        for(int packetCount = 0; packetCount < toGrabCount; packetCount++){
            //Indicate that data is being written and we need to wait until its fully written
            FENCE()

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

            //Indicate the space is free to write to in the input queue
            input.queues[qIndex].data[inputReadIndex].flow = 0;
            
            //Make sure everything is written/erased
            FENCE()
        }
    }
}

void multiPassPackets(int baseOutputQueuesIndex, int numOutputQueues, queue_t* mainQueue){
    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Tell the computer that we want to make sure this data is written
        FENCE()

        //Used for readability
        int mainReadIndex = (*mainQueue).toRead;

        //If there is no more data, then exit and get more packets
        if((*mainQueue).data[mainReadIndex].flow == 0){
            break;
        }

        //Get the appropriate output queue that the packet should go to.
        int qIndex = ((*mainQueue).data[mainReadIndex].flow % numOutputQueues) + baseOutputQueuesIndex;

        //Used for readability
        int outputWriteIndex = output.queues[qIndex].toWrite;

        //If the queue is full, then wait for a spot to open
        while(output.queues[qIndex].data[outputWriteIndex].flow != 0){
            ;
        }

        //Grab the packets data and move it into the output queue
        output.queues[qIndex].data[outputWriteIndex].order = (*mainQueue).data[mainReadIndex].order;
        output.queues[qIndex].data[outputWriteIndex].flow = (*mainQueue).data[mainReadIndex].flow;

        //Indicate the space is free to write to in the main queue
        (*mainQueue).data[mainReadIndex].flow = 0;

        //Indicate the next spot to write to in the output queue
        output.queues[qIndex].toWrite++;
        output.queues[qIndex].toWrite = output.queues[qIndex].toWrite % BUFFERSIZE;

        //Indicade the next spot to read from in the main queue
        (*mainQueue).toRead++;
        (*mainQueue).toRead = (*mainQueue).toRead % BUFFERSIZE;

        //Make sure everything is written/erased
        FENCE()
    }
}

void* movePackets(void* vargs){
    //Get the args into variables
    margs_t *args = (margs_t *)vargs;
    int numInputQueues = (*args).numInputQueuesAssigned;
    int baseInputQueueIndex = (*args).baseInputQueueNum;
    int numOutputQueues = (*args).numOutputQueuesAssigned;
    int baseOutputQueuesIndex = (*args).baseOutputQueueNum;

    //The amount of packets to grab from each queue. This algorithm gives all queues equal priority
    int toGrabCount = BUFFERSIZE / numInputQueues;

    //The main "wire" queue where everything will be written
    queue_t mainQueue;
    mainQueue.toRead = 0;
    mainQueue.toWrite = 0;

    //Start moving packets
    while(1){
        multiGrabPackets(toGrabCount, baseInputQueueIndex, numInputQueues, &mainQueue);
        multiPassPackets(baseOutputQueuesIndex, numOutputQueues, &mainQueue);
    }
}

void assignQueues(int numQueuesToAssign[], int baseQueuesToAssign[], int passerQueueCount, int queueCountTracker){
    int passerQueueCountTracker = passerQueueCount;
    int queueIndex = 0;
    int queueToPassRatio;
    for(int i = 0; i < passerQueueCount; i++){
        //Get the ratio of remaining in/out to remaining passer queues
        queueToPassRatio = (int)ceil((double)queueCountTracker / passerQueueCountTracker);

        //This is the number of in/out queues that the current passer queue should handle
        numQueuesToAssign[i] = queueToPassRatio;

        //Assign the base index. Only the base is needed as we can calculate the other indexes as they are contiguous
        baseQueuesToAssign[i] = queueIndex;

        //Adjust the number of remaining in/out and passer queues
        //remaining in/out queues decrease by variable amount
        //increase index of next available queue to the next free queue 
        //remaining Passer queues always decrease by 1
        queueCountTracker = queueCountTracker - queueToPassRatio;
        queueIndex = queueIndex + queueToPassRatio;
        passerQueueCountTracker--;
    }
}

void *run(void *argsv){
    //Determine if there are more input or output queues
    int passerQueueCount;
    int passerQueueCountTracker;
    if(input.queueCount <= output.queueCount){
        passerQueueCount = input.queueCount;
        passerQueueCountTracker = input.queueCount;
    }
    else{
        passerQueueCount = output.queueCount;
        passerQueueCountTracker = output.queueCount;
    }

    //determine the number of input queues each thread should get. 
    //The number of queues between threads should have a maximum difference of 1.   
    int numInputQueuestoAssign[passerQueueCount];
    int baseInputQueuesToAssign[passerQueueCount];
    int numOutputQueuestoAssign[passerQueueCount];
    int baseOutputQueuesToAssign[passerQueueCount];

    //Determine which input queues go with which passing thread
    assignQueues(numInputQueuestoAssign, baseInputQueuesToAssign, passerQueueCount, input.queueCount);

    //Determine which output queues go with which passing thread
    assignQueues(numOutputQueuestoAssign, baseOutputQueuesToAssign, passerQueueCount, output.queueCount);
    
    //Spawn enough threads that to match the lesser of the two.
    for(int i = 0; i < passerQueueCount; i++){
        //Assigned arguments to pass to threads
        algThreadArgs[i].numInputQueuesAssigned = numInputQueuestoAssign[i];
        algThreadArgs[i].baseInputQueueNum = baseInputQueuesToAssign[i];
        algThreadArgs[i].numOutputQueuesAssigned = numOutputQueuestoAssign[i];
        algThreadArgs[i].baseOutputQueueNum = baseOutputQueuesToAssign[i];

        //Create the thread
        Pthread_create(&moverThreadIds[i], NULL, movePackets, (void *)&algThreadArgs[i]);
    }
}