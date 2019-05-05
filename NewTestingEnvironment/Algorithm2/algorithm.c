#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"
#include "../FrameworkSRC/rte_memcpy.h"

#define ALGNAME "Bitwise Partition"

char* get_name(){
    return ALGNAME;
}

function get_input_thread(){
    return input_thread;
}

function get_output_thread(){
    return output_thread;
}

#define BUFFERLEN 1536

size_t partSize;
packet_t pktQueue[MAX_NUM_OUTPUT_THREADS][BUFFERLEN];

int inFlag[MAX_NUM_INPUT_THREADS];
int outFlag[MAX_NUM_OUTPUT_THREADS];

void * input_thread(void * args){
	pthread_detach(pthread_self());
	
	threadArgs_t *threadArgs = (threadArgs_t*) args;
	int core = threadArgs->coreNum;
	int threadID = threadArgs->threadNum;
	int outCount = outputThreadCount;

    //Set the thread to its own core
    set_thread_props(core, 2);

	while(partSize == 0);

	unsigned int toWrite[outCount]; // write pointer for partitions
	unsigned int startPart = (core - 2)*partSize; // which partition to write to in output queues
	unsigned int endPart = startPart + partSize - 1; // the last location in the partition
	
	// store starting partition size in array for fast access
	for(int i = 0; i < outCount; i++){
		toWrite[i] = startPart; // set write pointer to correct partition
	}
	
    register unsigned int g_seed0 = (unsigned int)time(NULL);
    register unsigned int g_seed1 = (unsigned int)time(NULL);
	
	unsigned int mask; //currFlow, length;
	unsigned int outMask;
	unsigned int flowNum[FLOWS_PER_THREAD] = {0};
	unsigned int offset = (core - 2)*FLOWS_PER_THREAD;
	
	// set mask
	if(outCount >= 7)
		mask = 15;
	else if(outCount >= 5)
		mask = 7;
	else if(outCount >= 3)
		mask = 3;	
	else//(outCount >= 1)
		mask = 1;
		
	//tsc_t start, end;
	int outIndx = outCount - 1;
	packet_t currPkt;
	
    input[threadID].readyFlag = 1;
	
	int counter = 0;
    //Wait until everything else is ready
    while(startFlag == 0);

	while(1){
		
		// *** FAST PACKET GENERATOR ***
		g_seed1 = (214013*g_seed1+2531011);
		//length = ((g_seed1>>16)&0x1FFF) + 64; //Min value 64: Max value 8191 + 64:
		currPkt.length = (((g_seed1>>16)&0x1FFF) + 64)/8; //Min value 64: Max value 8191 + 64:
		
		g_seed0 = (214013*g_seed0+2531011);
		//currFlow = ((g_seed0>>16)&0x0007) + offset + 1;//Min value offset + 1: Max value offset + 9:
		currPkt.flow = ((g_seed0>>16)&0x0007) + offset + 1;//Min value offset + 1: Max value offset + 9:
		
		currPkt.order = flowNum[currPkt.flow - offset - 1]++;
		// ************
		
		// find which output queue to write to using flow and mask
		if((outMask = currPkt.flow & mask) > outIndx)
			outMask = 0;
		
		while(pktQueue[outMask][toWrite[outMask]].flow != 0); // wait for space in partition
 
		memcpy(pktQueue[outMask][toWrite[outMask]].payload, &currPkt.payload, currPkt.length);
		//rte_memcpy(pktQueue[outMask][toWrite[outMask]].payload, &currPkt.payload, currPkt.length);
		
		/*
		counter = 141;
		while(counter){
			rte_mov64((uint8_t *) pktQueue[outMask][toWrite[outMask]].payload, (uint8_t *) &currPkt.payload);
			counter--;
			
		}
		*/
		
		pktQueue[outMask][toWrite[outMask]].length = currPkt.length;
		pktQueue[outMask][toWrite[outMask]].order = currPkt.order;
		pktQueue[outMask][toWrite[outMask]].flow = currPkt.flow;
		
		//rte_mov32((uint8_t *) &pktQueue[outMask][toWrite[outMask]], (uint8_t *) &currPkt);
		
		toWrite[outMask]++;
		
		if(toWrite[outMask] > endPart)
			toWrite[outMask] = startPart;
	}
	return NULL;
}

void * output_thread(void * args){
	pthread_detach(pthread_self());
	
	threadArgs_t *threadArgs = (threadArgs_t*) args;
	int core = threadArgs->coreNum;
	int threadID = threadArgs->threadNum;
	int outNum = core - 11; // since thread creations starts at core 6
	
	int inCount = inputThreadCount;
	int outCount = outputThreadCount;

    //Set the thread to its own core
    set_thread_props(core, 2);
	
	while(partSize == 0);
	
	unsigned int toRead[inCount]; // read pointer for partitions
	unsigned int startPart[inCount]; // holds starting partition indexes for the queues
	unsigned int endPart[inCount]; // stores ending position of partition
	
	// store starting partition size in array for fast access
	for(int i = 0; i < inCount; i++){
		startPart[i] = i*partSize;
		endPart[i] = startPart[i] + partSize - 1;
		toRead[i] = startPart[i];
	}
		
	// initialize queues and partitions
	for(int i = 0; i < outCount; i++){
		pktQueue[outNum][i].flow = 0;
	}
	
	unsigned int expected[inCount * FLOWS_PER_THREAD + 1];
	unsigned int currFlow;
	int readPart = 0;
	
	bzero(expected, sizeof(int) * (inCount*FLOWS_PER_THREAD+1));
	
	packet_t currPkt;
	
    output[threadID].readyFlag = 1;

    //Wait until everything else is ready
    while(startFlag == 0);
	
	while(1){
		
		// spin lock & cycles through partitions for available packets
		while(pktQueue[outNum][toRead[readPart]].flow == 0){
			readPart++;
			
			if(readPart >= inCount)
				readPart = 0;
		}
		
		memcpy(&currPkt, &pktQueue[outNum][toRead[readPart]], sizeof(packet_t));
		//rte_memcpy(&currPkt, &pktQueue[outNum][toRead[readPart]], sizeof(packet_t));
		
		currFlow = currPkt.flow;
		
		if(expected[currFlow] != currPkt.order){
            fprintf(stderr,"ERROR: Packet out of order in queue %d for flow %ld\n", outNum, currPkt.flow);						
            fprintf(stderr,"Expected: %d\n", expected[currFlow]);			
            fprintf(stderr,"Actual: %ld\n", currPkt.order);
			exit(0);
		}
		
		// process packet
		pktQueue[outNum][toRead[readPart]].flow = 0;
		
		//pktCount[outNum]++;
		output[threadID].byteCount += currPkt.length + 24;
		expected[currFlow]++;
		
		toRead[readPart]++;
		
		// check if we moved into next partition
		if(toRead[readPart] > endPart[readPart]){
			toRead[readPart] = startPart[readPart]; // reset reader to beginning of current partition
		
			readPart++; // move to next partition
			
			if(readPart >= inCount)
				readPart = 0;
			
		}
	}

	return NULL;
}

pthread_t * run(void *argsv){
    int inCount = inputThreadCount;
	
	//Initialize thread attributes
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);

    //Tell the system we are setting the schedule for the thread, instead of inheriting
    Pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED);
	
	// calculate cache lined partitions based on 1024 sized queue
	
	if((1024/inCount % 64) == 0)
		partSize = 1024/inCount;
	else
		partSize = (1024/inCount) + (64 - ((1024/inCount) % 64));
	
	return NULL;
}