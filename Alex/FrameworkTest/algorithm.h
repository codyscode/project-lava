#ifndef ALGORITHM_H
#define ALGORITHM_H

#include"global.h"

//Define all functions used in algorithm file here
void run(algoArgs_t*);   
void grabPackets(int, int, queue_t*);
void passPackets(int, int, queue_t*);

#endif

