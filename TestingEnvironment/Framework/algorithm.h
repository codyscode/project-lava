#ifndef ALGORITHM_H
#define ALGORITHM_H

#include"global.h"

//Define all functions used in any algorithm file here.
//Note: There is no overloading in c
void* run(void*);   

//Algorithm 1
void grabPackets(int, queue_t*);
void passPackets(queue_t*);

//Algorithm 2
void* movePackets(void* vargs);
void multiGrabPackets(int toGrabCount, int baseInputQueueIndex, int numInputQueues, queue_t* mainQueue);
void multiPassPackets(int baseOutputQueuesIndex, int numOutputQueues, queue_t* mainQueue);
void assignQueues(int numQueuesToAssign[], int baseQueuesToAssign[], int passerQueueCount, int queueCountTracker);
#endif

