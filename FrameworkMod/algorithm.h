#ifndef ALGORITHM_H
#define ALGORITHM_H

#include"global.h"

//Define all functions used in algorithm file here
void* run(void*);   
void grabPackets(int, queue_t*);
void passPackets(queue_t*);

#endif

