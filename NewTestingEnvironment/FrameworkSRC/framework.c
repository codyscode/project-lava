// *** FRAMEWORK ***
// This is meant to act as a test bed for algorithms that pass packets between two sets of threads.

// Times the algorithms and has an interface defined below for making algorithms "plug and play"

// Handle joining all threads spawned and returning the results of the run.

// Ensures no other processes are running

//



// *** TIMING ***
// Algorithm speed is measured in packets per second:

// Sampling is done by using the sig_alarm method with a 30 second timer set

// The user inidicates when to start the timer within their algorithm
// by setting a global variable telling the alarm to be set for 30 seconds

// It is the user's job to keep track of overhead time for spawning packets and passing them
// Overhead needs to be calculated fairly and accurately. We are trusting the user.


// *** REQUIRED IN ALGORITHM SRC FILE ****

// import global.h 
//   - Have access to built in queue structures and global variables

// pthread_t* run(void*)
//   - This will spawn any additional threads or do other work your algorithm may need
//   - Return NULL if no additional threads were spawned
//   - Return the array containing the array of thread ID's if you did spawn more threads

// char* getName()
//   - This will return the algorithm name for data purposes

// function get_input_thread()
//   - This will return the function pointer to the method that the input threads should run.

// function get_output_thread()
//   - This will return the function pointer to the method that the output threads should run.
   
// It is advised to assign threads to certain cores otherwise your algorithm could perform very poorly

// IMPORTANT: any additional threads you spawn in the pthread * run() function should return
// The variable endFlag will be set after the 30 second signal

#include"global.h" 
#include"wrapper.h"

#define sizeIgnore 9

void check_if_ideal_conditions(){
    if(!SUPPORTED_PLATFORM){
        printf("Unsupported Platform.\nExiting...\n");
        exit(1);
    }
    //Used for parseing running processes
    char * word = NULL;
    char * line = NULL;
    char lineCpy[1000];
    char response[10];
    size_t len = 0;
    ssize_t read;
    FILE *fptr;

    //Get the parent process ID into a string
    //This is the sudo call for the framework
    long ppID = (long)getppid();
    int ppIDlength = snprintf(NULL, 0, "%ld", ppID);
    char* ppidString = Malloc(ppIDlength + 1);
    snprintf(ppidString, ppIDlength + 1, "%ld", ppID);

    //Get the processID into a string
    //This is what is called from sudo
    long pID = (long) getpid();
    int pIDlength = snprintf(NULL, 0, "%ld", pID);
    char* pidString = Malloc(pIDlength + 1);
    snprintf(pidString, pIDlength + 1, "%ld", pID);
    
    //If a process contains one of these, then it is ok to run in the background
    char * toIgnore[sizeIgnore] = {"USER", "bash", "sh", "ps", "tty1", "vim", "grep", pidString, ppidString};

    //Used for the command line argument
    char * fileName = "temp";
    char * cmd = "rm -f temp; ps -au >> temp";

    //Check to see if a shell is open
    if(system(NULL) == 0){
        printf("Shell Not Available, Cannot Check Processes. Exiting...");
        exit(0);
    }
    else{
        //Execute the command to list processes
        system(cmd);

        //Open the file that the data was stored in
        if(access(fileName, F_OK) != -1){
            fptr = Fopen(fileName, "r+");
        }

        //Go line by line and look for if another process is running that would degrade performance
        while ((read = getline(&line, &len, fptr)) != -1) {
            strcpy(lineCpy, line);
            //Cycle through each word in the line using tokenization
            word = strtok(line, " -\t\n");
            int ignore = 0;

            //Go word by word
            while(word != NULL && ignore == 0){
                //If the word is an ignore word, then the process is ok to exist
                for(int i = 0; i < sizeIgnore; i++){
                    //If the strings are the same then the line can be ignored
                    if(strcmp(word, toIgnore[i]) == 0){
                        ignore = 1;
                        break;
                    }
                }

                //move onto the next word in the line
                word = strtok(NULL, " -\n");
            }

            //If we make it out of the loop and we havent found a word signaling the process is ok to run
            //Then we have a process that shouldn't be running and we should not run the framework yet
            if(ignore == 0){
                printf("Another process is running that will skew the results.\n");
                printf("Running Process: %s\n", lineCpy);
                printf("Would you like to proceed(y/n): ");
                scanf("%s", response);
                if(response != NULL){
                    if(strcmp(response, "y") == 0){
                        printf("\n");
                        continue;
                    }
                    else{
                        printf("Exiting...\n");
                        //Delete the temporary file
                        int status = remove(fileName);

                        if (status != 0){
                            printf("Unable to delete temporary file: temp\n");
                            perror("Following error occurred");
                        }  

                        exit(1);
                    }
                }
                else{
                    printf("Exiting...\n");
                    //Delete the temporary file
                    int status = remove(fileName);

                    if (status != 0){
                        printf("Unable to delete temporary file: temp\n");
                        perror("Following error occurred");
                    }  
                    exit(1);
                }
                
            }
        }

        //Close the file
        fclose(fptr);

        //Free the line variable
        free(line);

        //Delete the temporary file
        int status = remove(fileName);

        if (status != 0){
            printf("Unable to delete the file\n");
            perror("Following error occurred");
        }   
    }
}

void init_built_in_queues(){
    //initialize all values for built in input/output queues to 0
    for(int qIndex = 0; qIndex < inputThreadCount; qIndex++){
        for(int dataIndex = 0; dataIndex < BUFFERSIZE; dataIndex++){
            input[qIndex].queue.data[dataIndex].packet.flow = 0;
            input[qIndex].queue.data[dataIndex].packet.order = 0;
            input[qIndex].queue.data[dataIndex].packet.length = 0;
            input[qIndex].queue.data[dataIndex].isOccupied = NOT_OCCUPIED;

            output[qIndex].queue.data[dataIndex].packet.flow = 0;
            output[qIndex].queue.data[dataIndex].packet.order = 0;
            output[qIndex].queue.data[dataIndex].packet.length = 0;
            output[qIndex].queue.data[dataIndex].isOccupied = NOT_OCCUPIED;
        }
        input[qIndex].queue.toRead = 0;
        input[qIndex].queue.toWrite = 0;
        
        output[qIndex].queue.toRead = 0;
        output[qIndex].queue.toWrite = 0;

        input[qIndex].count = 0;

        output[qIndex].count = 0;
    }
}

void spawn_input_threads(pthread_attr_t attrs, function input_thread){
    //Core for each input thread to be assigned to
    int core = 2;

    //Spawn the input threads and pass appropriate arguments
    for(int index = 0; index < inputThreadCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        //input[index].threadArgs.queue = &input[index].queue;
        input[index].threadArgs.threadNum = index;
        input[index].threadArgs.coreNum = core;
        core++;

        //Spawn input thread
        Pthread_create(&input[index].threadID, &attrs, input_thread, (void *)&input[index].threadArgs);

        //Detach the thread
        Pthread_detach(input[index].threadID);
    }

    //Indicate to user that input threads have spawned
    printf("\nSpawned %lu input thread(s)\n", inputThreadCount);
}

void spawn_output_threads(pthread_attr_t attrs, function processing_thread){
    //Core for each output thread to be assigned to
    int core = 11;

    //Spawn the output threads and pass appropriate arguments
    for(int index = 0; index < outputThreadCount; index++){
        //Initialize Thread Arguments with first queue and number of queues
        //output[index].threadArgs.queue = &output[index].queue;
        output[index].threadArgs.threadNum = index;
        output[index].threadArgs.coreNum = core;
        core++;

        //Spawn the thread
        Pthread_create(&output[index].threadID, &attrs, processing_thread, (void *)&output[index].threadArgs);
        
        //Detach the thread
        Pthread_detach(output[index].threadID);
    }

    //Indicate to user that output threads have spawned
    printf("Spawned %lu output thread(s)\n", outputThreadCount);
}

void check_threads(){
    int tries;
    //Ensure that every thread is ready before starting the timer
    //This ensures the user sets the flag properly and acts as a barrier for timing
    for(int i = 0; i < inputThreadCount; i++){
        tries = 0;
        while(input[i].readyFlag == 0){
            usleep(1000000);
            tries++;
            if(tries == 10){
                printf("\nAlgorithm fails to set ready flags for input threads in time.\nMake sure to set input[threadNum].readyFlag = 1 when your input thread is ready to start passing.\nExiting...\n");
                exit(1);
            }
        }
        printf("Input Thread %d:   Ready - Running on Core %lu\n", i, input[i].threadArgs.coreNum);
    }
    for(int i = 0; i < outputThreadCount; i++){
        tries = 0;
        while(output[i].readyFlag == 0){
            usleep(1000000);
            tries++;
            if(tries == 10){
                printf("\nAlgorithm fails to set ready flags for output threads in time.\nMake sure to set output[threadNum].readyFlag = 1 when your output thread is ready to start passing.\nExiting...\n");
                exit(1);
            }
        }
        printf("Output Thread %d:  Ready - Running on Core %lu\n", i, output[i].threadArgs.coreNum);
    }
}

void wait_for_threads(){
    size_t prevCount = 0;
    size_t count = 0;
    int timer = RUNTIME;
    while(endFlag == 0){
        if(timer % 2 == 0){
            for(int i = 0; i < outputThreadCount; i++){
                count += output[i].count;
            }
            printf("\x1b[A\rEstimated: \t %'lu bits per second\n", ((count - prevCount) * 4));
            printf("Time Remaining:  %d Seconds  ", timer);
            fflush(NULL);
            prevCount = count;
            count = 0;
            timer--;    
        }   
        else{
            printf("\rTime Remaining:  %d Seconds  ", timer);
            fflush(NULL);
            timer--;  
        }
        usleep(1000000);
    }
}

void output_data(){
    //Get the algorithm name
    char* algName = get_name();

    //Print to the user that the tests ran successfully
    printf("Success, passed all packets in order!\n");
	
    //Create the file to write the data to
    FILE *fptr;
    char fileName[10000];
	
    //append .csv to algorithm name
    snprintf(fileName, sizeof(fileName),"%s.csv", algName);

    //Output the data to the user
    printf("\nAlgorithm %s passed %.3f Gbs on average.\n", algName, (double)((finalTotal/RUNTIME) * 8) / 1000000000);

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
    fprintf(fptr, "%s,%lu,%lu,%lu\n", algName, inputThreadCount, outputThreadCount, (finalTotal/RUNTIME) * 8);
    fclose(fptr);
}

int main(int argc, char**argv){
    //Error checking for proper command line arguments
    if (argc < 3){
        printf("Usage: sudo ./framework <# input threads>, <# output threads>\n");
        exit(0);
    }

    //Used for formatting numbers with commas
    setlocale(LC_NUMERIC, "");

    //Ensure that no other process is running in the background. 
    //If the user passes a flag indicating that they dont care about background processes
    //then dont run this code.
    if (argc < 4){
        check_if_ideal_conditions();
    }

    //Assign the main thread to run on the first core and dont change its scheduling
    set_thread_props(0, 2);

    //Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Initialize the stop/start flags for the algorithm
    startFlag = 0;
    endFlag = 0;

    //initialize ready signal flags for threads
    for(int i = 0; i < MAX_NUM_INPUT_THREADS; i++){
        input[i].readyFlag = 0;
    }

    //Grab the number of input and output threads to use
    inputThreadCount = atoi(argv[1]);
    outputThreadCount = atoi(argv[2]);

    //Make sure that the number of input and output threads is valid
    assert(inputThreadCount <= MAX_NUM_INPUT_THREADS);
    assert(outputThreadCount <= MAX_NUM_OUTPUT_THREADS);
    assert(inputThreadCount >= MIN_INPUT_THREAD_COUNT && outputThreadCount >= MIN_OUTPUT_THREAD_COUNT);

    //Initialize the sets of queues to 0
    init_built_in_queues();

    printf("Spawning Threads:\n");

    spawn_input_threads(attrs, get_input_thread());

    spawn_output_threads(attrs, get_output_thread());

    //Indicate to the user that the tests are starting
    printf("\nStarting Metric for Algorithm: %s\n", get_name());

    //Call the users run method which handles:
    // - Spawn any additional threads their algorithm may need
    // - Return the array of the spawned threads
    pthread_t *extraThreads;
    extraThreads = run(NULL);

    //Setup the alarm
    alarm_init();

    //Indicate we are waiting for threads to be ready
    printf("\nWaiting for Threads to be Ready:\n\n");

    check_threads();

    printf("\nAll Threads Ready. *** Starting Passing ***\n\n\n");
    fflush(NULL);

    //Set all count variables to 0 to prevent "cheating"
    for(int i = 0; i < MAX_NUM_INPUT_THREADS; i++){
        if(output[i].count > 0){
            printf("Counting started before Timer, Results are not valid. Exiting...\n");
            exit(1);
        }
    }

    //Reset final results
    finalTotal = 0;

    //Start the alarm and set start flag to signal all threads to start
    alarm_start();

    //Wait for threads to finish and print out to the user estimates
    //of how their algorithm is doing
    wait_for_threads();

    //Wait for any threads that were spawed in the run function to finish
    if(extraThreads != NULL){
        int size = sizeof(*extraThreads)/sizeof(pthread_t);
        for(int i = 0; i < size; i++){
            Pthread_join(extraThreads[i], NULL);
        }
    }

    //Output all data to user and files
    output_data();

    return 1;
} 