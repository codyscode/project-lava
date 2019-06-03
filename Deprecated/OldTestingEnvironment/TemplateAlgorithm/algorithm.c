/*
-This is a template for how an algorithm should look
-You can add more functions if you want, just make sure they join at the end otherwise your program will go into an inifite loop
-The entire framework exits when void *run(void *args) exits.
-Most of this stuff is required, however if you find a way to do something that does the same functionality you can do that.
-Be sure to define your algorithm name up top
*/

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

#define ALGNAME "insert alg name here"

//Define any arguments your threads your need will to perform the algorithm
typedef struct ThreadArgs{
    int coreNum;
}threadArgs;

void* exampleFunction(void *args){
    //Initialize any variables

    //This is recommended to have unless the number of extra threads created exceeds 16
    //int nextAvailableCore = (int)args->coreNum;
    //set_thread_props(nextAvailableCore);

    //Wait for the alarm to start
    while(startFlag == 0);

    //While the alarm hasnt gone off do work
    //Using branch prediction, this should theorteically only fail on the first iteration of the loop
    //and the last interation of the loop, so its our best bet as of now
    while(endFlag == 0){
        //Code for passing packets
    }

    return NULL;
}

void *run(void *argsv){
    //Set the thread to its own core
    //+1 is neccessary as we have core 0, input threads, output threads
    int nextAvailableCore = input.queueCount + output.queueCount + 1
    set_thread_props(nextAvailableCore);
    nextAvailableCore++;

    //Initialize the alarm
    alarm_init();

    //Spawn any threads here, 
    //call set_thread_props(nextAvailableCore) and increment the variable for each thread made
    int numThreads = 0;
    pthread_t workerIds[numThreads];
    threadArgs workerArgs[numThreads];

    //Create the threads
    for(int i = 0; i < input.queueCount; i++){
        workerArgs[i] = nextAvailableCore;
        nextAvailableCore++;
		Pthread_create(&workerIds[i], NULL, exampleFunction, NULL;
	}

    //Start the alarm after your threads have spawned
    //The alarm will set endFlag to 1 once it goes off
    alarm_start();

    //Wait for each thread you spawned to halt upon reading endFlag
    for(int i = 0; i < numThreads; i++){
		pthread_join(threadCount[i], NULL);  
    }

    return NULL
}

//This function is required for the framework and should not be modified
char* getName(){
    return ALGNAME;
}