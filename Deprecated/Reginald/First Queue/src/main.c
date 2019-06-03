// this code if for a 1-to-1 buffer transfer
// it assumes that both the input and output queue is of the same size
// it assumes that the output queue starts off empty

//////////// the segment of code was taken from Ian's example-queue.c code ///////////////////
typedef unsigned long long tsc_t;

// Get the current value of the TSC.  This is a rolling 64 bit counter
// and its frequency varies from across x64 CPU models so we have to
// calibrate it.

#if defined(__i386__)

// Not actually used - most chips are x64 nowadays.
static __inline__ tsc_t rdtsc(void)
{
    register tsc_t x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static __inline__ tsc_t rdtsc(void)
{
    register unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (tsc_t)lo)|( ((tsc_t)hi)<<32 );
}

#endif

#define _GNU_SOURCE
////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>

#include "scope.h"

#define NUM_IN_BUFS 4
#define MAX_BUF_IN 20
#define MAX_BUF_OUT 20 

// moving data from input to output buffers
// buffers contain random integer which defined which flow it belongs to
// using FIFO for processing out buffer
int in[NUM_IN_BUFS][MAX_BUF_IN];
int out[MAX_BUF_OUT];

int writeTo = 0; // index used to keep track of last written data which is set by the in buffers
int errs = 0;

void *inToOut(void *arg);
void *procOutBuf(void *arg);

pthread_mutex_t writeTo_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
	pthread_t tid[NUM_IN_BUFS+1];

	pthread_create(&tid[NUM_IN_BUFS], NULL, procOutBuf, NULL);
	
	int val;
	for(int i = 0; i < NUM_IN_BUFS; i++)
	{
		val = 0;
		// load up input queue's
		for(int j = 0; j < MAX_BUF_IN; j++)
		{
			in[i][j] = (i+1)*MAX_BUF_IN + val++;	
			//printf("in[%d][%d]: %d\n", i, j, in[i][j]);
		}
		
		pthread_create(&tid[i], NULL, inToOut, &in[i]);
	}

	for(int i = 0; i <= NUM_IN_BUFS; i++)
		pthread_join(tid[i], NULL);
	
	
	
}

// write to out buffer from in buffer
// dealing on with indexes whose contents are empty of the out buffer
void *inToOut(void *arg)
{
	int *in = (int *)arg;
	int i = 0;
	
	printf("Processing buffer...\n");
	while(i < MAX_BUF_IN)
	{	
		if(writeTo != MAX_BUF_OUT)
		{
			pthread_mutex_lock(&writeTo_mutex);
			if(out[writeTo] == 0)
			{
				out[writeTo++] = in[i];
				//printf("in[%d] = %d out[%d] = %d\n", i, in[i],writeTo-1, out[writeTo-1]);
				in[i++] = 0;
				//printf("in[%d] = %d out[%d] = %d\n", i-1, in[i-1],writeTo-1, out[writeTo-1]);
			}
			pthread_mutex_unlock(&writeTo_mutex);
		}
	}

	
	return NULL;
}


// processes out buffer
// No locking mechanism required since only the filled contents of the out buffer are altered
// inToOut() deals with the empty section of the buffer
void *procOutBuf(void *arg)
{	
	//pthread_detach(pthread_self());
	int lastProc = 0, procTo, expectVal;
	
	int counter[NUM_IN_BUFS];
	for(int i = 0; i < NUM_IN_BUFS; i++)
		counter[i] = 0;
	
	for(;;)
	{
		pthread_mutex_lock(&writeTo_mutex);
		
		procTo = writeTo;
		if(writeTo == MAX_BUF_OUT)
			writeTo = 0;
		
		pthread_mutex_unlock(&writeTo_mutex);
		
		while(lastProc < procTo && out[lastProc] != 0 && lastProc < MAX_BUF_OUT)
		{	
			// write to the end of buffer 
			if(lastProc > procTo)
				procTo = MAX_BUF_OUT;
			
			expectVal = (out[lastProc]/MAX_BUF_IN) ;
			expectVal = (expectVal * MAX_BUF_IN) + counter[expectVal-1]++;
			
			if(out[lastProc] != expectVal)
			{
				fprintf(stderr, "At index %d: Actual: %d Expected: %d\n", lastProc, out[lastProc], expectVal);
				errs++;
			}
			else
				printf("At index %d: Actual: %d Expected: %d\n", lastProc, out[lastProc], expectVal);
			
			out[lastProc++] = 0;
			//printf("lastProc: %d procTo: %d\n", lastProc, procTo);
		}
		
		if(lastProc == MAX_BUF_OUT)
			lastProc = 0;
		//printf("Errors: %d\n", errs);
	}
	
	return NULL;
}


