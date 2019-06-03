#ifndef ALGORITHM_H
#define ALGORITHM_H

#include"global.h"

//Define all functions used in any algorithm file here
void* run(void*);   
char* getName(void);

//Algorithm1
void grabPackets(int, queue_t*);
void passPackets(queue_t*);

//Algorithm2
void assignQueues(int numQueuesToAssign[], int baseQueuesToAssign[], int passerQueueCount, int queueCountTracker);
void* movePackets(void* vargs);
void multiPassPackets(int baseOutputQueuesIndex, int numOutputQueues, queue_t* mainQueue);
void multiGrabPackets(int toGrabCount, int baseInputQueueIndex, int numInputQueues, queue_t* mainQueue);

//processingLimitTest
void* determineSpeedOutput(void* args);
void* testProcessing(void* args);

//inputLimitTest
void* testInputGeneration(void* args);
void* determineSpeedInput(void* args);

#endif

