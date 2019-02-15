/*
-This is a sample algorithm that doesnt actually pass, it just checks to make sure the packets generate by the input are in valid order
-The wrapper.h file includes wrapper for pthread functions
-The global.h file includes all the global variables/helper functions for framework that arent thread functions
 It also includes the declaration of the run function
*/

#include"global.h"
#include"wrapper.h"

void grabPackets(int inputQueueCount, int toGrabCount, queue_t* mainQueue){
    //Go through each queue and grab the stated amount of packets from it
    for(int qIndex = 0; qIndex < inputQueueCount; qIndex++){
        for(int packetCount = 0; packetCount < toGrabCount; packetCount++){
            //Used for readability
            int mainWriteIndex = (*mainQueue).toWrite;
            int inputReadIndex = queues[qIndex].toRead;

            //If there is no where to write then start processing
            if((*mainQueue).data[mainWriteIndex].flow != 0){
                return;
            }

            //If there is nothing to grab then move to next queue
            if(queues[qIndex].data[inputReadIndex].flow == 0){
                break;
            }

            //Grab the packets data and move it into the mainQueue
            (*mainQueue).data[mainWriteIndex].order = queues[qIndex].data[inputReadIndex].order;
            (*mainQueue).data[mainWriteIndex].flow = queues[qIndex].data[inputReadIndex].flow;

            //Indicate the next spot to read from in the input queue
            queues[qIndex].toRead++;
            queues[qIndex].toRead = queues[qIndex].toRead % BUFFERSIZE;

            //Indicade the next spot to write to in the main queue
            (*mainQueue).toWrite++;
            (*mainQueue).toWrite = (*mainQueue).toWrite % BUFFERSIZE;

            //Indicate the space is free to write to in the input queue
            queues[qIndex].data[inputReadIndex].flow = 0;
        }
    }
}

void passPackets(int outputQueueCount, int offset, queue_t* mainQueue){
    //Go through mainQueue and write its contents to the appropriate output queue
    while(1){
        //Used for readability
        int mainReadIndex = (*mainQueue).toRead;

        //If there is no more data, then exit and get more packets
        if((*mainQueue).data[mainReadIndex].flow == 0){
            return;
        }

        //Get the appropriate output queue that this should be sending to
        int qIndex = (*mainQueue).data[mainReadIndex].flow % outputQueueCount;
        qIndex = qIndex + offset;

        //Used for readability
        int outputWriteIndex = queues[qIndex].toWrite;

        //If the queue is full, then wait for it to become unfull
        while(queues[qIndex].data[outputWriteIndex].flow != 0){
            ;
        }

        //Grab the packets data and move it into the output queue
        queues[qIndex].data[outputWriteIndex].order = (*mainQueue).data[mainReadIndex].order;
        queues[qIndex].data[outputWriteIndex].flow = (*mainQueue).data[mainReadIndex].flow;

        //Indicate the next spot to write to in the output queue
        queues[qIndex].toWrite++;
        queues[qIndex].toWrite = queues[qIndex].toWrite % BUFFERSIZE;

        //Indicade the next spot to read from in the main queue
        (*mainQueue).toRead++;
        (*mainQueue).toRead = (*mainQueue).toRead % BUFFERSIZE;

        //Indicate the space is free to write to in the main queue
        (*mainQueue).data[mainReadIndex].flow = 0;
    } 
}

void run(algoArgs_t *args){
    //Get arguments into variables
    int inputQueueCount = (*args).inputQueueCount; 
    int outputQueueCount = (*args).outputQueueCount; 

    //The first output queue index in the queues array is inputQueueCount
    //(i.e if inputQueueCount = 2 -> first index of output queue = 2)
    int offset = inputQueueCount;

    //The amount of packets to grab from each queue. This algorithm gives all queues equal priority
    int toGrabCount = BUFFERSIZE / inputQueueCount;

    //The main "wire" queue where everything will be written
    queue_t mainQueue;
    mainQueue.toRead = 0;
    mainQueue.toWrite = 0;

    while(1){
        grabPackets(inputQueueCount, toGrabCount, &mainQueue);
        passPackets(inputQueueCount, toGrabCount, &mainQueue);
    }
}