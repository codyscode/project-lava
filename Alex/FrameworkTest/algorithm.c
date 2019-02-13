/*
-This is a sample algorithm that doesnt actually pass, it just checks to make sure the packets generate by the input are in valid order
-The wrapper.h file includes wrapper for pthread functions
-The global.h file includes all the global variables/helper functions for framework that arent thread functions
 It also includes the declaration of the run function
-There should be an associated algorithm.h file if you add any other functions besides run
*/

#include<global.h>
#include<wrapper.h>

void run(algoArgs_t *args){
    //Get arguments into variables
    int inputQueuesCount = (*args).inputQueueCount; 

    //------SINGLE THREAD FOR CONFIRMING VALID INPUT-------
    int currIndices[inputQueuesCount];

    //Set all start indices for the position in the queue to read to 0 initially
    for(int i = 0; i < inputQueuesCount; i++){
        currIndices[i] = 0;
    }

    //Array used for checking if the packets are in the appropriate order
    //[Thread index][flow index]
    int nextExpectedOrder[inputQueuesCount][5 * inputQueuesCount + 1];

    //set all intial expected values to 0
    for(int i = 0; i < inputQueuesCount; i++){
        for(int j = 0; j < 5 * inputQueuesCount; j++){
            nextExpectedOrder[i][j] = 0;
        }
    }

    int reads = 0;
    int maxReads = 10000;
    
    //Master thread that reads out the input queues to make sure they can be read in order
    while(reads < maxReads){
        //Read each input queue and output results to confirm they are in order
        for(int queuesIndex = 0; queuesIndex < inputQueuesCount; queuesIndex++){
            int index = currIndices[queuesIndex];

            //While there is data to read in the queue, then read it
            while(queues[queuesIndex].data[index].flow != 0){
                int currFlow = queues[queuesIndex].data[index].flow - 1;

                //If the order is out of line (i.e the flow number is not what was expected) then break
                if(nextExpectedOrder[queuesIndex][currFlow] != queues[queuesIndex].data[index].order){
                    printf("Packet: Flow: %lu | Order: %lu\n", queues[queuesIndex].data[index].flow, queues[queuesIndex].data[index].order);
                    printf("Packet out of order. Expected %d | Got %lu\n", nextExpectedOrder[queuesIndex][currFlow], queues[queuesIndex].data[index].order);
                    exit(1);
                }
                //Otherwise increase next expected order number for the next packet in the flow
                else{
                    nextExpectedOrder[queuesIndex][currFlow]++;
                }

                //"Process" the packet
                printf("Queue: %d | Flow: %lu | Order: %lu\n", queuesIndex, queues[queuesIndex].data[index].flow, queues[queuesIndex].data[index].order);
                
                //Tell the queue that the position is free
                queues[queuesIndex].data[index].flow = 0;
                
                //Increment to the next spot to check in the curent queue
                index++;
                index = index % BUFFERSIZE;
                
                //Increase the number of reads
                reads++;
                if(reads > maxReads){
                    break;
                }
            }

            //Once the queue runs out of memory, save where it was last read and move onto next queue
            if(reads < maxReads){
                printf("No data currently available moving to next thread\n");
                currIndices[queuesIndex] = index;
            }
            //IF we are past the max number of breaks, then exit the program
            else{
                break;
            }
        }
    }
    printf("\n-------------FINISHED-------------\n\nAll packets were in correct order\n");
}