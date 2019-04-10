//-This is meant to act as a test bed for algorithms that pass packets between two "processes".
// --Processes is a relative term here that just refers to two separate entities doing their own work.
//-The framework consists of input and output queues that serve as the buffers to write and read into respectivley
//-with the input queues representing a queue in a NIC and the output queue representing a buffer to put the packets
//-into for the other process to read


//-Algorithm speed is measured in packets per second:
// --Sampling is done by using the sig_alarm method
// --The user inidicates when to start the timer within their algorithm
//   by setting a global variable telling the alarm to be set for 30 seconds
// --The generate packets function within gloabl.c keeps track of the overhead in
//   the time it takes to generate packets as a local variable and in the end the
//   total overhead is subtracted from the 30 second alarm


//-REQUIRED in algorithm.c:
// --void* run(void*)
//   ---This will spawn input and output threads that handle each queue
// --char* getName()
//   ---This will return the algorithm name for data purposes
// --function get_fill_method()
//   ---This will return the proper way to fill a custom queue or standard queue
//   ---return NULL if using default input queues
// --function get_drain_method()
//   ---This will return the proper way to drain a custom queue or standard queue
//   ---return NULL if using default output queues
// --import global.h 
//   ---Have access to the queue structures
// --It is advised to assign threads to certain cores
//   otherwise your algorithm could perform very poorly
// --IMPORTANT: To have your thread exit properly instead of doing
//   Instead of:    while(1){ //constant work  } 
//   Do:            while(endFlag == 0){ //constant work  }

#include"global.h" 
#include"wrapper.h"

//The job of the input threads is to make packets to populate the buffers. As of now the packets are stored in a buffer.
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
    int orderForFlow[FLOWS_PER_QUEUE] = {0};
    int currFlow, currLength;
    int offset = queueNum * FLOWS_PER_QUEUE;
	
    //Continuously generate input numbers until the buffer fills up. 
    //Once it hits an entry that is not empty, it will wait until the input is grabbed.
    int index = 0;
    unsigned int g_seed = (unsigned int)time(NULL);

    //--Uncomment for loop for Lazy/Fast Method -- Faster for n m where n. m > 1, slower for 1 1
    /*
    //Setup all data in a queue to pull from and get random values in the queue
    for(size_t index = 0; index < BUFFERSIZE; index++){
        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = (rand_r(&g_seed) % FLOWS_PER_QUEUE) + offset + 1;

        //Assign a random length to the packet. Length defines the entire packet struct, not just payload
        //Minimum size computed below is MIN_PACKET_SIZE
        //Maximum size computed below is MAX_PACKET_SIZE
        currLength = rand_r(&g_seed) % (MAX_PAYLOAD_SIZE + 1 - MIN_PAYLOAD_SIZE) + MIN_PAYLOAD_SIZE;

        //Create the new packet and store it in the queue
        (*inputQueue).data[index].packet.order = orderForFlow[currFlow - offset - 1];
        (*inputQueue).data[index].packet.length = currLength;
        (*inputQueue).data[index].packet.flow = currFlow;

        (*inputQueue).data[index].isOccupied = OCCUPIED;

        //Update the next flow number to assign
        orderForFlow[currFlow - offset - 1]++;
    }*/

    //After having the base queue fill up, maintain all the packets in the queue by just overwriting their order number
    while(1){
        //Fast Thread Independent Psuedo Random Number Generator
        g_seed = (214013*g_seed+2531011); //-Comment Out For Lazy/Fast Method

        //Assign a random flow within a range: [n, n + 1, n + 2, n + 3, n + 4]. +1 is to avoid the 0 flow
        currFlow = ((g_seed>>16)&0x7FFF % FLOWS_PER_QUEUE) + offset + 1; //-Comment Out For Lazy/Fast Method

        //Assign a random length to the packet. Length defines the entire packet struct, not just payload
        //Minimum size computed below is MIN_PACKET_SIZE
        //Maximum size computed below is MAX_PACKET_SIZE
        currLength = (g_seed>>16)&0x7FFF % ((MAX_PAYLOAD_SIZE + 1 - MIN_PAYLOAD_SIZE) + MIN_PAYLOAD_SIZE); //-Comment Out For Lazy/Fast Method

        //If the queue spot is filled then that means the input buffer is full so continuously check until it becomes open
        while((*inputQueue).data[index].isOccupied == OCCUPIED){
            ;//Do Nothing until a space is available to write
        }

        //Create the new packet. Write the order and length first before flow as flow > 0 indicates theres a packet there
        (*inputQueue).data[index].packet.order = orderForFlow[currFlow - offset - 1];
        (*inputQueue).data[index].packet.length = currLength; //-Comment Out For Lazy/Fast Method
        (*inputQueue).data[index].packet.flow = currFlow; //-Comment Out For Lazy/Fast Method

        //Update the next flow number to assign
        orderForFlow[currFlow - offset - 1]++; //-Comment Out For Lazy/Fast Method
        //orderForFlow[(*inputQueue).data[index].packet.flow - offset - 1]++; //-UNComment Out For Lazy/Fast Method

        //Say that the spot is ready to be read
        (*inputQueue).data[index].isOccupied = OCCUPIED;

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
    int numInputs = (int)inputQueueCount;   
    queue_t *processQueue = (queue_t *)processArgs->queue;

    //Set the thread to its own core
    set_thread_props(processArgs->coreNum);

    //"Process" packets to confirm they are in the correct order before consuming more. 
    //Processing threads process until they get to a spot with no packets
    int expected[numInputs * FLOWS_PER_QUEUE + 1]; 
    int index; 

    //Initialize array to 0. Because it is dynamically allocated at runtime we need to use memset() or a for loop
    memset(expected, 0, (numInputs * FLOWS_PER_QUEUE + 1) * sizeof(int));

    //Go through each space in the output queue until we reach an emtpy space in which case we swap to the other queue to process its packets
    while(1){
        index = (*processQueue).toRead;

        //If there is no packet, continuously check until something shows up
        while((*processQueue).data[index].isOccupied == NOT_OCCUPIED){
            ;//Do Nothing until there is a packet to read
        }

        //Get the current flow for the packet
        int currFlow = (*processQueue).data[index].packet.flow;

        // set expected order for given flow to the first packet that it sees
        if(expected[currFlow] == 0){
            expected[currFlow] = (*processQueue).data[index].packet.order;
        }
		
        //Packets order must be equal to the expected order.
        //Implementing less than currflow causes race conditions with writing
        //Any line that starts with a * is ignored by python script
        if(expected[currFlow] != (*processQueue).data[index].packet.order){
            //Print out the contents of the processing queue that caused an error
            for(int i = 0; i < BUFFERSIZE; i++){
                printf("*Position: %d, Flow: %ld, Order: %ld\n", i, (*processQueue).data[i].packet.flow, (*processQueue).data[i].packet.order);
            }
            
            //Print out the specific packet that caused the error to the user
            printf("Error Packet: Flow %lu | Order %lu\n", (*processQueue).data[index].packet.flow, (*processQueue).data[index].packet.order);
            printf("Packet out of order in Output Queue %d. Expected %d | Got %lu\n", queueNum, expected[currFlow], (*processQueue).data[index].packet.order);
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

            //Set the position to free. Say it has already processed data
            (*processQueue).data[index].isOccupied = NOT_OCCUPIED;
        }
    }
}

void check_if_ideal_conditions(){
    if(!SUPPORTED_PLATFORM){
        printf("Unsupported Platform.\nExiting...\n");
        exit(1);
    }
    #define sizeIgnore 7
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE *fptr;

    //Get the parent process ID into a string
    long ppID = (long)getppid();
    int ppIDlength = snprintf(NULL, 0, "%ld", ppID);
    char* ppidString = Malloc(ppIDlength + 1);
    snprintf(ppidString, ppIDlength + 1, "%ld", ppID);

    //Get the processID into a string
    long pID = (long) getpid();
    int pIDlength = snprintf(NULL, 0, "%ld", pID);
    char* pidString = Malloc(pIDlength + 1);
    snprintf(pidString, pIDlength + 1, "%ld", pID);
    
    //If a process contains one of these, then it is ok to run in the background
    char * toIgnore[sizeIgnore] = {"USER", "bash", "sh", "ps", "tty1", pidString, ppidString};

    //Used for the command line argument
    char * fileName = "temp";
    char * cmd = "rm -f temp; ps -au >> temp";

    //Check to see if a shell is open
    if(system(NULL) == 0){
        printf("Shell Not Available, Cannot Check Processes. Exiting...");
        exit(0);
    }
    else{
        //Execute the command
        system(cmd);

        //Open the file that the data was stored in
        if(access(fileName, F_OK) != -1){
            fptr = Fopen(fileName, "r");
        }

        //Go line by line and look for if another process is running that would degrade performance
        while ((read = getline(&line, &len, fptr)) != -1) {
            //Cycle through each word in the line using tokenization
            char * word;
            word = strtok (line, " -\n");
            int ignore = 0;
            while (word != NULL && ignore == 0){
                //If the word is an ignore word, then the process is ok to exist
                for(int i = 0; i < sizeIgnore; i++){
                    //If the strings are the same then the line can be ignored
                    if(strcmp(word, toIgnore[i]) == 0){
                        ignore = 1;
                        break;
                    }
                }

                //move onto the next word in the line
                word = strtok (NULL, " -\n");
            }

            //If we make it out of the loop and we havent found a word signaling the process is ok to run
            //Then we have a process that shouldn't be running and we should not run the framework yet
            if(ignore == 0){
                printf("Another process is running that will skew the results.\nExiting...\n");
                exit(1);
            }
            ignore = 0;
        }

        //Close the file
        fclose(fptr);

        //Free the line variable
        if (line){
            free(line);
        }

        //Delete the temporary file
        int status = remove(fileName);

        if (status != 0){
            printf("Unable to delete the file\n");
            perror("Following error occurred");
        }   
    }
}

void init_queue_sets(){
    //initialize input queues to all 0 values
    for(int qIndex = 0; qIndex < inputQueueCount; qIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            input[qIndex].queue.data[dataIndex].packet.flow = 0;
            input[qIndex].queue.data[dataIndex].packet.order = 0;
            input[qIndex].queue.data[dataIndex].packet.length = 0;
            input[qIndex].queue.data[dataIndex].isOccupied = NOT_OCCUPIED;
        }
        input[qIndex].queue.toRead = 0;
        input[qIndex].queue.toWrite = 0;
    }

    //initialize ouput queues to all 0 values
    for(int qIndex = 0; qIndex < outputQueueCount; qIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            output[qIndex].queue.data[dataIndex].packet.flow = 0;
            output[qIndex].queue.data[dataIndex].packet.order = 0;
            output[qIndex].queue.data[dataIndex].packet.length = 0;
            output[qIndex].queue.data[dataIndex].isOccupied = NOT_OCCUPIED;
        }
        output[qIndex].queue.toRead = 0;
        output[qIndex].queue.toWrite = 0;
    }
}

void fill_queues(function custom_fill){
    if(custom_fill != NULL){
        custom_fill();
    }
    //Allow input buffers to fill before starting algorithm
    for(int i = 0; i < inputQueueCount; i++){
        while(input[i].queue.data[BUFFERSIZE-1].flow != FREE_SPACE_TO_WRITE);
    }

    //Indicate to the user that the tests are starting
    printf("\nStarting Metric...\n");
}

void drain_queues(function custom_drain){
    if(custom_drain != NULL){
        custom_drain();
    }
    //Check if first and last position of queues are empty
    //This works since queues are written in a sequential order
    for(int i = 0; i < outputQueueCount; i++){
        while(output[i].queue.data[FIRST_INDEX].flow != FREE_SPACE_TO_WRITE); 
        while(output[i].queue.data[LAST_INDEX].flow != FREE_SPACE_TO_WRITE);
    }
}

void output_data(){
    //Used for formatting numbers with commas
    setlocale(LC_NUMERIC, "");

    //Get the algorithm name
    char* algName = getName();

    //Indicate to the user that we are printing the results
    printf("\nResults:\n");

    size_t finalCount = 0;
    //Output how many packets each output queue processed
    for(int qIndex = 0; qIndex < outputQueueCount; qIndex++){
        printf("Queue %d: %ld packets passed in %d seconds\n", qIndex, output[qIndex].queue.count, RUNTIME);
        finalCount += output[qIndex].queue.count;
    }

    //Print to the user that the tests ran successfully
    printf("\nSuccess, passed all packets in order!\n");
	
    //Create the file to write the data to
    FILE *fptr;
    char fileName[10000];
	
    //append .csv to algorithm name
    snprintf(fileName, sizeof(fileName),"%s.csv", algName);

    //if the file alreadty exists, open it
    if(access(fileName, F_OK) != -1){
        fptr = Fopen(fileName, "a");
    }
    //if the file does not exit, create one, then assign the appropriate head to the .csv file
    else{
        fptr = Fopen(fileName, "a");
        fprintf(fptr, "Algorithm,Input,Output,Packet\n");
    }	
	
    //Output the data to the file
    fprintf(fptr, "%s,%lu,%lu,%lu\n", algName, inputQueueCount, outputQueueCount, finalCount/RUNTIME);
    fclose(fptr);
	
    //Output the data to the user
    printf("\nAlgorithm %s passed %'lu packets per second\n", algName, finalCount/RUNTIME);
}

int main(int argc, char**argv){
    //Error checking for proper command line arguments
    if (argc < 3){
        printf("*Usage: Queues <# input queues>, <# output queues>\n");
        exit(0);
    }

    //Ensure that no other process is running in the background
    check_if_ideal_conditions();

    //Assign the main thread to run on the first core and dont change its scheduling
    assign_to_zero();

    //Initialize the stop/start flags for the algorithm
    startFlag = 0;
    endFlag = 0;

    //Grab the number of input and output queues to use
    inputQueueCount = atoi(argv[1]);
    outputQueueCount = atoi(argv[2]);

    //Make sure that the number of input and output queues is valid
    assert(inputQueueCount <= MAX_NUM_INPUT_QUEUES);
    assert(outputQueueCount <= MAX_NUM_OUTPUT_QUEUES);
    assert(inputQueueCount >= MIN_INPUT_QUEUE_COUNT && outputQueueCount >= MIN_OUTPUT_QUEUE_COUNT);

    //Initialize the sets of queues to 0
    init_queue_sets();

    //Wait for the queues to fill up before passing
    fill_queues(get_fill_method());

    //Indicate to the user that the tests are starting
    printf("\nStarting Metric...\n");

    //Call the users run method which handles:
    // -spawning the threads for dealing with input
    // -spawning the threads that deal with processing output
    // -Dealing with passing packets to the other sets of queues
    run(NULL);

    //wait until the first and last position of all processing queues are empty
    //Because we are looking at packets passed, instead of processed and the output queues
    //maintain the count, we wait until the output queues finish counting all the packets passed to them
    drain_queues(get_drain_method());

    //Output all data to user and files
    output_data();

    return 1;
} 