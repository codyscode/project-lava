/* --------algo1.c------------
-This is a sample algorithm that doesnt actually pass, it just checks to make sure the packets generate by the input are in valid order
-The wrapper.h file includes wrapper for pthread functions
-The global.h file includes all the global variables/helper functions for framework that arent thread functions
 It also includes the declaration of the run function
*/

#include"global.h"
#include"wrapper.h"

void grabPackets(int toGrabCount, queue_t* mainQueue){
    //Go through each queue and grab the stated amount of packets from it
    for(int qIndex = 0; qIndex < input.queueCount; qIndex++){
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

void passPackets(queue_t* mainQueue){
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

void *run(void *argsv){
    //The amount of packets to grab from each queue. This algorithm gives all queues equal priority
    int toGrabCount = BUFFERSIZE / input.queueCount;

    //The main "wire" queue where everything will be written
    queue_t mainQueue;
    mainQueue.toRead = 0;
    mainQueue.toWrite = 0;

    while(1){
        grabPackets(toGrabCount, &mainQueue);
        passPackets(&mainQueue);
    }
}