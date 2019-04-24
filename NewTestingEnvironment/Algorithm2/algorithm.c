#include "../FrameworkSRC/global.h"
#include "../FrameworkSRC/wrapper.h"

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

size_t partSize = 1024;
packet_t pktQueue[MAX_NUM_OUTPUT_THREADS][BUFFERLEN];

int inFlag[MAX_NUM_INPUT_THREADS];
int outFlag[MAX_NUM_OUTPUT_THREADS];

void * input_thread(void * args){
	pthread_detach(pthread_self());
	
	threadArgs_t *threadArgs = (threadArgs_t*) args;
	int core = threadArgs->coreNum;
	int threadID = threadArgs->threadNum;
	int outCount = outputThreadCount;
	//int threadID = core-2;

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
	
	inFlag[core-2] = 1;
	printf("Set inflag for %d\n", core-2);
	printf("Mask = %d\n", mask);
	fflush(stdout);
	
    input[threadID].readyFlag = 1;

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
		
		//start = rdtsc();
		
		// find which output queue to write to using flow and mask
		//// use packet pointer instead???
		if((outMask = currPkt.flow & mask) > outIndx)
			outMask = 0;
		
		while(pktQueue[outMask][toWrite[outMask]].flow != 0); // wait for space in partition
 
		memcpy(((char*)&pktQueue[outMask][toWrite[outMask]].data), ((char *)&currPkt.data), currPkt.length);
		//memcpy(&(pktQueue[outMask][toWrite[outMask]].data), &(currPkt.data), currPkt.length);
		//memcpy(&pktQueue[outMask][toWrite[outMask]], &currPkt, 24);
		
		//memcpy64(((char*)&pktQueue[outMask][toWrite[outMask]]) + 24, ((char *)&currPkt) + 24, currPkt.length);
		
		pktQueue[outMask][toWrite[outMask]].length = currPkt.length;
		pktQueue[outMask][toWrite[outMask]].order = currPkt.order;
		pktQueue[outMask][toWrite[outMask]].flow = currPkt.flow
		//memcpy(&pktQueue[outMask][toWrite[outMask]], &currPkt, 24);
		//memcpy64(&pktQueue[outMask][toWrite[outMask]], &currPkt, 3);
		
		toWrite[outMask]++;
		
		if(toWrite[outMask] > endPart)
			toWrite[outMask] = startPart;
		
		//end = rdtsc();
		
		//inOverhead[threadID] += end - start;
		//inPktCount[threadID]++;
		//input[threadID].overhead++;
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
		printf("Startpart: %d\n", startPart[i]);
		printf("Endpart: %d\n", endPart[i]);
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
	
	outFlag[outNum] = 1;
	printf("Set outflag for %d\n", core-6);
	fflush(stdout);
	
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
		
		currFlow = pktQueue[outNum][toRead[readPart]].flow;
		
		if(expected[currFlow] != pktQueue[outNum][toRead[readPart]].order){
            fprintf(stderr,"ERROR: Packet out of order in queue %d for flow %d\n", outNum, currFlow);						
            fprintf(stderr,"Expected: %d\n", expected[currFlow]);			
            fprintf(stderr,"Actual: %ld\n", pktQueue[outNum][toRead[readPart]].order);
			exit(0);
		}
		
		// process packet
		pktQueue[outNum][toRead[readPart]].flow = 0;
		
		//pktCount[outNum]++;
		output[threadID].count++;
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
	int outCount = outputThreadCount;
	
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
	
	/*
	pthread_t tid;
	int *threadNum;
	
	// spawn input threads
	for(int i = 0; i < inCount; i++){
		threadNum = malloc(sizeof(int));
		*threadNum = i+2;
		Pthread_create(&tid, &attrs, input_thread, (void *)threadNum);
	}
	
	// spawn output threads
	for(int i = 0; i < outCount; i++){
		threadNum = malloc(sizeof(int));
		*threadNum = i+6;
		Pthread_create(&tid, &attrs, processing_thread, (void *)threadNum);
	}
	*/
	/*
	alarm_init();
	
	puts("Waiting for thread startup");
	fflush(stdout);
	
	// wait for start up for input threads
	for(int i = 0; i < inCount; i++)
		while(!inFlag[i]);
	
	// wait for start up for output threads
	for(int i = 0; i < outCount; i++)
		while(!outFlag[i]);
	
	alarm_start();
	
	puts("Starting timer...");
	while(!endFlag);
	*/
	//usleep(1000000);
	//puts("Alarm recieved...");

	/*
	unsigned int totalPkts = 0;
	
	for(int i = 0; i < inCount; i++){
		printf("Queue %d: %d packets generated\n", i, inPktCount[i]);
		totalPkts += inPktCount[i];
	}
	printf("\nTotal packets generated for all queues for %d seconds: %d\n\n", RUNTIME, totalPkts);
	
	totalPkts = 0;
	
	for(int i = 0; i < outCount; i++){
		printf("Queue %d: %d packets processed\n", i, pktCount[i]);
		totalPkts += pktCount[i];
	}
	
	printf("\nTotal packets processed for all queues for %d seconds: %d\n", RUNTIME, totalPkts);
*/	
}