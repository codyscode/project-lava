#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <signal.h>
#include <pthread.h>

typedef void* (*function)(void * args);

// Get the current value of the TSC.  This is a rolling 64 bit counter
// and its frequency varies from across x64 CPU models so we have to
// calibrate it.

typedef unsigned long long tsc_t;

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

#define numThreads 7
#define PADDING 4

typedef struct Sample{
    tsc_t overhead;
    size_t count;
}sample_t;

pthread_t threadID[numThreads];

sample_t samples[numThreads * PADDING];

size_t endFlags[numThreads * PADDING * 2];

size_t spawnedFlag[numThreads];

pthread_attr_t attrs;

//Defines how many ticks an algorithm should run for
#define RUNTIME 66000000000LL

void assign_to_zero(){
    pthread_t self = pthread_self();

    //Repin this thread to one specific core
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    if(pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
	    perror("pthread_attr_setaffinity_np");
    }
}

void set_thread_props(int tgt_core){
    pthread_t self = pthread_self();
    
    const struct sched_param params = {
	    .sched_priority = 2
    };

    if(pthread_setschedparam(self, SCHED_FIFO, &params)) {
	    perror("pthread_setschedparam");
    }

    //Repin this thread to one specific core
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(tgt_core, &cpuset);

    if(pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
	    perror("pthread_attr_setaffinity_np");
    }
}

void * test(void * args){
    printf("This function was called from a variable pointer\n\n");      
}

function get_input_function(){
    return test;
}

tsc_t testing(int index, sample_t *counter){
    // *** START TIMING TEST SECTION ***
    // This Generates roughly 1 packet every 27 cycles
    int count = 100000000; //100,000,000
    tsc_t startTSC = 0, endTSC = 0;

    int currFlow, length;
    int offset = numThreads;

    register unsigned int g_seed0 = (unsigned int)time(NULL);
    register unsigned int g_seed1 = (unsigned int)time(NULL);
    counter->overhead = 0;
    
    int i;
    while(endFlags[index * PADDING] == 0){
        // *** FASTEST/ACCURATE TIMING ***
        startTSC = rdtsc();

        // *** FASTEST PACKET GENERATOR ***
        g_seed0 = (214013*g_seed0+2531011);   
        currFlow = ((g_seed0>>16)&0x0007) + offset + 1;//Min value offset + 1: Max value offset + 9:
        g_seed1 = (214013*g_seed1+2531011); 
        length = ((g_seed1>>16)&0x1FFF) + 64; //Min value 64: Max value 8191 + 64:

        // *** FASTEST/ACCURATE TIMING ***
        endTSC = rdtsc();
        counter->overhead += endTSC - startTSC;
        counter->count++;
    }
    return counter->overhead;
}

void * testRunnable(void * args){
    //Assign it to a specific core
    int core = *((int *)args);
    set_thread_props(core);

    spawnedFlag[core - 3] = 1;

    //do 100,000,000 reads to rdtsc per thread
    tsc_t result = testing(core - 3, &samples[(core - 3) * PADDING]);
    printf("Thread %d Overhead Time: %ld ticks -- %lf Seconds\n\n", core - 3, result, (double)result / 2200000000);  

    return NULL;
}

void threadSpawner(){
    int cores[8] = {3, 4, 5, 6, 7, 8, 9, 10};
    int index;

    for(index = 0; index < numThreads; index++){
        //Spawn the thread
        endFlags[index * PADDING] = 0;
        spawnedFlag[index] = 0;
    }

    for(index = 0; index < numThreads; index++){
        //Spawn the thread
        pthread_create(&threadID[index], &attrs, testRunnable, (void *)&cores[index]);
    }  

    for(index = 0; index < numThreads; index++){
        while(spawnedFlag[index] == 0);
        //Wait for the thread to be spawned
    }
}

void overview(){
    register tsc_t total;
    register tsc_t start = 0;
    size_t countTotal = 0;
    size_t countRecord[numThreads];
    int index;

    printf("TESTING\n\n");

    //Constantly check to ensure that we are under the 66,000,000,000 ticks limit
    start = rdtsc();
    while(rdtsc() - start < RUNTIME){
        total = 0;
        for(index = 0; index < numThreads; index++){
            //take samples for when we potentially exit
            total += samples[index * PADDING].overhead;
            countRecord[index] = samples[index * PADDING].count;
        }
    }

    for(index = 0; index < numThreads; index++){
        countTotal += countRecord[index];
        endFlags[index * PADDING] = 1;
    }

    for(index = 0; index < numThreads; index++){
        pthread_join(threadID[index], NULL);
    }

    printf("Finished\nPackets Generated: \t%ld\n", countTotal);  
    printf("Runtime: \t\t%ld\n", RUNTIME * numThreads); 
    printf("Overhead Ticks: \t%ld\n\n", total); 
}

int main(int argc, char**argv){
    assign_to_zero();

    // *** RETURNING FUNCTION TEST ***
    void * args = NULL;
    function func = get_input_function();
    func(NULL);

    // *** TESTING MEASURING ***
    pthread_t overviewThreadID;
    pthread_attr_init(&attrs);

    //Spawn the threads
    threadSpawner();

    //Keep time
    overview();
}
