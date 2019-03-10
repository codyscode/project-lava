//This is meant to act as a test bed for algorithms that pass packets between two spaces.
//The framework consists of input and output queues that serve as the "connection" between the two spaces,
//with the input queues representing a queue in a NIC and the output queue representing passing to the other
//workload in order

// -Algorithm speed is measured by the main function.
// -The algorithm allows the run function to go for 3 seconds, measures the count, run for 10 seconds, then measure the count.
//  Packets per second is equal to (second count - first count) / 10.
//REQUIRED:
// -algorithm.c that defines a function void* run(void*)
// -algorithm.c that defines a function char* getName()
// -algorithm.h that defines the functions used in algorithm.c
// -algorithm.c needs to import global.h in order to have access to queues
// -alogrithm.c has access to count[] which is an array to keep track of the count.
//  count is an array to allow multiple threads to access any count variable
// - You will need to set thread props for the run function, otherwise your algorithm will perform very poorly

#include"global.h"
#include"wrapper.h"
#include"algorithm.h"

//The job of the input threads is to make packets to populate the buffers. As of now the packets are stored in a buffer.
//--Need to work out working memory simulation for packet retrieval--
//Attributes:
// -Each input queue generates packets in a circular buffer
// -Input queue stops writing when the buffer is full and continues when it is empty
// -Each input queue generates 5 flows (i.e input 1: 1, 2, 3, 4, 5; input 2: 6, 7, 8, 9, 10)
// -The flows can be scrambled coming from a single input (i.e stream: 1, 2, 1, 3, 4, 4, 3, 3, 5)
void* input_thread(void *args){
    //Get arguments and any other functions for input threads
    threadArgs_t *inputArgs = (threadArgs_t *)args;

    int queueNum = inputArgs->queueNum;   
    queue_t * inputQueue = (queue_t *)inputArgs->queue;

    //Set the thread to its own core
    set_thread_props(inputArgs->coreNum);

    //Each input buffer has 5 flows associated with it that it generates
    //-------------MODIFY THIS TO size_t---------------
    int flowNum[FLOWS_PER_QUEUE] = {0};
    int currFlow, currLength;
    int offset = queueNum * FLOWS_PER_QUEUE;
	
    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    int index = 0;
	int seed = time(NULL);
    while(1){
        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = (rand_r(&seed) % FLOWS_PER_QUEUE) + offset + 1;

        //Assign a random length to the packet. Length defines the entire packet struct, not just payload
        //Minimum size computed below is MIN_PACKET_SIZE
        //Maximum size computed below is MAX_PACKET_SIZE
        currLength = rand_r(&seed) % (MAX_PAYLOAD_SIZE + 1 - MIN_PAYLOAD_SIZE) + MIN_PAYLOAD_SIZE;
		
        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while((*inputQueue).data[index].flow != 0){
            ;
        }

        //Create the new packet. Write the order and length first before flow as flow > 0 indicates theres a packet there
        (*inputQueue).data[index].order = flowNum[currFlow - offset - 1];
        (*inputQueue).data[index].length = currLength;
        (*inputQueue).data[index].flow = currFlow;

        //Update the next flow number to assign
        flowNum[currFlow - offset - 1]++;

        //Update the next spot to be written in the queue
        index++;
        index = index % BUFFERSIZE;

        //Increment the number of packets generated
        (*inputQueue).count++;
    }
}

//The job of the processing threads is to ensure that the packets are being delivered in proper order by going through
//output queues and reading the order
//--Need to work out storing in memory for proper simulation--
void* processing_thread(void *args){
    //Get arguments and any other functions for input threads
    threadArgs_t *processArgs = (threadArgs_t *)args;

    int queueNum = processArgs->queueNum;
    int numInputs = (int)input.queueCount;   
    queue_t *processQueue = (queue_t *)processArgs->queue;

    //Set the thread to its own core
    set_thread_props(processArgs->coreNum);

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    int expected[numInputs * FLOWS_PER_QUEUE + 1]; 
    int index; 
    int threadCompleted = 0;

    //Initialize array to 0. Because it is dynamically allocated at runtime we need to use memset() or a for loop
    memset(expected, 0, (numInputs * FLOWS_PER_QUEUE + 1) * sizeof(int));

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(1){
        index = (*processQueue).toRead;

        //If there is no packet, continuously check until something shows up
        while((*processQueue).data[index].flow == 0){
            ;
        }

        //Get the current flow for the packet
        int currFlow = (*processQueue).data[index].flow;

		// set expected order for given flow to the first packet that it sees
        //-------------REDEFINE FLOWS SUCH THAT THEY RESET TO SIMULATE REAL WORLD PACKETS--------
        //-------------IF A 0 IS SEEN IN FLOW, THEN RESET EXPECTED[FLOW] TO 0---------
		if(expected[currFlow] == 0){
			expected[currFlow] = (*processQueue).data[index].order;
		}
		
        //Packets order must be equal to the expected order.
        //Implementing less than currflow causes race conditions with writing
        //Any line that starts with a * is ignored by python script
        if(expected[currFlow] != (*processQueue).data[index].order){
            
            for(int i = 0; i < BUFFERSIZE; i++){
                //printf("*Position: %d, Flow: %ld, Order: %ld\n", i, (*processQueue).data[i].flow, (*processQueue).data[i].order);
            }
            
            printf("*Error Packet: Flow %lu | Order %lu\n", (*processQueue).data[index].flow, (*processQueue).data[index].order);
            printf("*Packet out of order in Output Queue %d. Expected %d | Got %lu\n", queueNum, expected[currFlow], (*processQueue).data[index].order);
            exit(0);
        }
        //Else "process" the packet by filling in 0 and incrementing the next expected
        else{            
            //increment the number of packets passed
            (*processQueue).count++;

            //Set what the next expected packet for the flow should be
            expected[currFlow]++;

            //Move to the next spot in the outputQueue to process
            (*processQueue).toRead++;
            (*processQueue).toRead = (*processQueue).toRead % BUFFERSIZE;

            //Set the position to free
            (*processQueue).data[index].flow = 0;
        }
    }
}

void main(int argc, char**argv){
    //Error checking for proper running
    if (argc < 3){
        printf("*Usage: Queues <# input queues>, <# output queues>\n");
        exit(0);
    }

    assign_to_zero();
	
    //Get the algorithm name
    char* algName = getName();

    //Initialize the stop/start flags for the algorithm
    startFlag = 0;
    endFlag = 0;

    //Grab the number of input and output queues to use
    input.queueCount = atoi(argv[1]);
    output.queueCount = atoi(argv[2]);

    //Make sure that the number of input and output queues is valid
    assert(input.queueCount <= MAX_NUM_INPUT_QUEUES);
    assert(output.queueCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(input.queueCount > 0 && output.queueCount > 0);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Tell the system we are setting the schedule for the thread, instead of inheriting
    if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
        perror("pthread_attr_setinheritsched");
    }

    //initialize input queues
    for(int queueIndex = 0; queueIndex < input.queueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            input.queues[queueIndex].data[dataIndex].flow = 0;
            input.queues[queueIndex].data[dataIndex].order = 0;
        }
        input.queues[queueIndex].toRead = 0;
        input.queues[queueIndex].toWrite = 0;
    }

    //initialize ouput queues
    for(int queueIndex = 0; queueIndex < output.queueCount; queueIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            output.queues[queueIndex].data[dataIndex].flow = 0;
            output.queues[queueIndex].data[dataIndex].order = 0;
        }
        output.queues[queueIndex].toRead = 0;
        output.queues[queueIndex].toWrite = 0;
    }

    int mutexErr;
    //Initialize input locks
    for(int index = 0; index < MAX_NUM_INPUT_QUEUES; index++){
        Pthread_mutex_init(&input.locks[index], NULL);
    }

    //Initialize output locks
    for(int index = 0; index < MAX_NUM_OUTPUT_QUEUES; index++){
        Pthread_mutex_init(&output.locks[index], NULL);
    }

    int pthreadErr;
    int core = 1;
    //create the input generation threads.
    //Each input queue has a thread associated with it
    for(int index = 0; index < input.queueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        input.threadArgs[index].queue = &input.queues[index];
        input.threadArgs[index].queueNum = index;
        input.threadArgs[index].coreNum = core;
        core++;

        //Spawn input thread
        Pthread_create(&input.threadIDs[index], &attrs, input_thread, (void *)&input.threadArgs[index]);

        //Detach the thread
        Pthread_detach(input.threadIDs[index]);
    }

    //create the output processing threads.
    //Each output queue has a thread associated with it
    for(int index = 0; index < output.queueCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        output.threadArgs[index].queue = &output.queues[index];
        output.threadArgs[index].queueNum = index;
        output.threadArgs[index].coreNum = core;
        core++;

        //Spawn the thread
        Pthread_create(&output.threadIDs[index], &attrs, processing_thread, (void *)&output.threadArgs[index]);

        //Detach the thread
        Pthread_detach(output.threadIDs[index]);
    }

    //Print out testing information to the user
    printf("\nTesting with: \n%lu Input Queues \n%lu Output Queues\n\n", input.queueCount, output.queueCount);

    //Allow buffers to fill
	for(int i = 0; i < input.queueCount; i++){
		while(input.queues[i].data[BUFFERSIZE-1].flow != 0);
	}

    //Run thread ID
    pthread_t runID;

    //Run the algorithm
    run(NULL);
	
	// check if first and last position of queues are empty
	// this works since queues are written in a sequential order
	for(int i = 0; i < output.queueCount; i++){
		while(output.queues[i].data[0].flow != 0); 
		while(output.queues[i].data[BUFFERSIZE-1].flow != 0);
		
	}
	
    size_t finalCount = 0;
    for(int i = 0; i < output.queueCount; i++){
		printf("Queue %d: %ld packets passed in %d seconds\n", i, output.queues[i].count, RUNTIME);
        finalCount += output.queues[i].count;
    }

    //Output the data to a file
	printf("Success, passed all packets in order!\n");
	
	FILE *fptr;
	char fileName[10000];
	
	// append .csv to algorithm name
	snprintf(fileName, sizeof(fileName),"%s.csv", algName);
	
	// file exists
	if(access(fileName, F_OK) != -1){
		fptr = Fopen(fileName, "a");
	}
	// file does not exit, append header
	else{
		fptr = Fopen(fileName, "a");
		fprintf(fptr, "Algorithm,Input,Output,Packet\n");
	}	
	
    fprintf(fptr, "%s,%lu,%lu,%lu\n", algName, input.queueCount, output.queueCount, finalCount/RUNTIME);
	fclose(fptr);
	
	printf("%s,%lu,%lu,%lu\n", algName, input.queueCount, output.queueCount, finalCount/RUNTIME);
} 