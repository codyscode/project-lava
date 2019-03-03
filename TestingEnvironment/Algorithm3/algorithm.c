// This algorithm is meant to run from m to m queues or a 1:1 ratio
// the purpose of this algorithm is to test the signals and timer functionality

#include"../Framework/global.h"
#include"../Framework/wrapper.h"

#define ALGNAME "Test_Timer"

static void *movePackets(void *args);

/*--Old Code-- Defined in global.h
int startFlag = 0; // flag used to start moving packets
int endFlag = 0; // flag used to end algorithm
*/
size_t missedPackets = 0;

/*--Old Code-- Defined in global.c
void sig_alrm(int signo)
{
	endFlag = 1;
}
*/

void *run(void *argsv){
	
	if(input.queueCount != output.queueCount){
		fprintf(stderr, "ERROR: this algorithm only runs for 1:1 ratio m to m queues\n");
		exit(1);
	}
	
	//--New Code--
	alarm_init();
	/*--Old Code--Defined in global.c
 	struct sigaction act; //structure used to handle signals
	act.sa_handler = sig_alrm; // set signal handler to function sig_alarm
	
	if(sigemptyset(&act.sa_mask) < 0){
		perror("ERROR: sigemptyset() failed");
		exit(1);
	}
	act.sa_flags = 0; // no flags used
	
	// set SIGALRM to be handled specified by act
	if(sigaction(SIGALRM, &act, NULL) < 0){
		perror("ERROR: sigaction() failed");
		exit(1);
	}
	*/
	
	pthread_t tid[input.queueCount];
	int *id;	
	
	for(int i = 0; i < input.queueCount; i++){
		id = Malloc(sizeof(int));
		*id = i;
		Pthread_create(&tid[i], NULL, movePackets, (void *) id);
	}

	//--New code--
	start_alarm();
	/* --Old code--Defined in global.c
	startFlag = 1; // start moving packets
	alarm(RUNTIME); // set alarm for RUNTIME seconds
	*/
	
	for(int i = 0; i < input.queueCount; i++)
		pthread_join(tid[i], NULL);  

	if(missedPackets > 0)
		fprintf(stderr,"Dropped packets not accounted for: %ld\n", missedPackets);
	
	return NULL;
}

static void *movePackets(void *args){
		
	int id = *((int *) args);
	free(args);
	
	queue_t *in = &input.queues[id];
	queue_t *out = &output.queues[id];
	
	// initialize variables
	size_t *read = &(in->toRead);
	size_t *write = &(in->toWrite);
	
	*read = 0;
	*write = 0;
	
	while(startFlag == 0); // don't start moving packets until all threads have been created and initialized
	
	while(endFlag == 0){		
		// check if there's something in the input buffer 
		if(in->data[*read].flow != 0){
			// if space if available in the output buffer 
			if(out->data[*write].flow == 0){
				
				// copy data, order, the flow to out buffer			
				memcpy(&(out->data[*write].data), &(in->data[*read].data), in->data[*read].length);
				out->data[*write].order = in->data[*read].order;
				FENCE() // add barrier so previous instruction are written before flow is written
				out->data[*write].flow = in->data[*read].flow;
								
				// empty packet from input queue
				in->data[*read].flow = 0;
				
				// increment input and output queues read and write locations
				++(*write);
				++(*read);
				(*write) = (*write) % BUFFERSIZE;
				(*read) = (*read) % BUFFERSIZE;
			}
			/*
			// this is used as a check to see how many packets can be passed optimally by the algorithm
			else{
				//printf("WARNING: dropped packet, make sure framework accounts for this\n");
				missedPackets++;
				//in->data[*read].flow = 0;
				//++(*read);
			}
			*/	
		}
	}
	
	return NULL;
}

char* getName(){
    return ALGNAME;
}