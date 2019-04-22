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
#include <limits.h>

typedef void* (*function)(void * args);

// Get the current value of the TSC.  This is a rolling 64 bit counter
// and its frequency varies from across x64 CPU models so we have to
// calibrate it.

typedef unsigned long long tsc_t;

#define FENCE() \
   __asm__ volatile ("mfence" ::: "memory");

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

//Defines how many microseconds an algorithm should run for
#define RUNTIME 5000000
#define THREADS 2
#define SSIZE 2048
#define FLOWS_PER_THREAD 8

typedef struct Packet{
    size_t flow;
    int sequence;
    size_t order;
    char data[9000];
}packet_t;

typedef struct ThreadData{
    tsc_t overhead;
    size_t count[FLOWS_PER_THREAD];
    size_t result[FLOWS_PER_THREAD];
}data_t;

typedef struct ThreadArgs{
    data_t *results;
    int core;
}args_t;

int readyFlag[THREADS];
int startFlag;
int endFlag;
pthread_t threadID[THREADS];
args_t threadArgs[THREADS];
data_t threadData[THREADS];
pthread_attr_t attrs;

//List of sequences that the packets must follow. Used for varification
//Sequences vary in length
int sequencers[THREADS * FLOWS_PER_THREAD][SSIZE];

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

void generate_sequences(int amount){
    int seqIndex = 0;
    
    //No seed so results are consistent across algorithms
    while(seqIndex < amount * FLOWS_PER_THREAD){
        //Generate a sequence for each flow
        for(int i = 0; i < SSIZE; i++){
            sequencers[seqIndex][i] = rand();
        }

        //Move onto the next sequence
        seqIndex++;
    }
}

int verify_sequence(int flow, int count, size_t seqResult){
    printf("Flow: %'d \tCount: %'d\tThread Result: %'lu\t", flow, count, seqResult);
    //Generate a sequence for each input thread
    size_t answer = INT_MAX;
    int seqIndex = 0;
    for(int i = 0; i < count; i++){
        answer = answer & sequencers[flow][seqIndex];
        answer = answer | sequencers[flow][seqIndex];
        seqIndex++;
        seqIndex = seqIndex % SSIZE;
    }

    printf("Actual Result: %'lu\n", answer);

    if(answer == seqResult)
        return 1;
    else
        return 0;
}

void * thread(void * args){
    //Assign it to a specific core
    args_t *targs = (args_t *)args;
    int core = targs->core;
    set_thread_props(core);

    // *** SEQUENCE TESTER ***
    tsc_t startTSC;
    int offset = (core - 3) * FLOWS_PER_THREAD;

    register unsigned int g_seed0 = (unsigned int)time(NULL);

    register tsc_t overhead = 0;
    register int flowIndex = 0;

    size_t count[FLOWS_PER_THREAD] = {0};
    int seqIndex[FLOWS_PER_THREAD] = {0};
    int seq[FLOWS_PER_THREAD] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX , INT_MAX};

    packet_t packet;
    data_t *records = targs->results;

    readyFlag[core - 3] = 1;

    while(startFlag == 0);

    while(endFlag == 0){
        // *** START TIMER ***
        startTSC = rdtsc();

        // *** PACKET GENERATOR ***
        g_seed0 = (214013*g_seed0+2531011);  
        flowIndex = ((g_seed0>>16)&0x0007);
        packet.flow = flowIndex + offset;//Min value: 0 + offset || Max value: 7 + offset
        packet.sequence = sequencers[flowIndex + offset][seqIndex[flowIndex]];

        seqIndex[flowIndex]++;
        if(seqIndex[flowIndex] >= SSIZE) seqIndex[flowIndex] = 0;
        // *** END PACKET GENERATOR ***

        // *** END TIMER ***
        overhead += rdtsc() - startTSC;

        // *** PACKET PROCESSOR ***
        seq[packet.flow - offset] = (seq[packet.flow - offset] & packet.sequence) | packet.sequence;
        count[packet.flow - offset]++;
        // *** END PACKET PROCESSOR ***
    }

    records->overhead = overhead;
    for(int i = 0; i < FLOWS_PER_THREAD; i++){
        records->count[i] = count[i];
        records->result[i] = seq[i];
    }

    return NULL;
}

void * thread_2(void * args){
    //Assign it to a specific core
    args_t *targs = (args_t *)args;
    int core = targs->core;
    set_thread_props(core);

    // *** SEQUENCE TESTER ***
    tsc_t startTSC;
    int offset = (core - 3) * FLOWS_PER_THREAD;

    register unsigned int g_seed0 = (unsigned int)time(NULL);

    register tsc_t overhead = 0;
    register int flowIndex = 0;

    size_t count[FLOWS_PER_THREAD] = {0};
    int order[FLOWS_PER_THREAD] = {0};
    int expected[FLOWS_PER_THREAD] = {0};

    packet_t packet;
    data_t *records = targs->results;

    readyFlag[core - 3] = 1;

    while(startFlag == 0);

    while(endFlag == 0){
        // *** START TIMER ***
        startTSC = rdtsc();

        // *** PACKET GENERATOR ***
        g_seed0 = (214013*g_seed0+2531011);  
        flowIndex = ((g_seed0>>16)&0x0007);
        packet.flow = flowIndex + offset;//Min value: 0 + offset || Max value: 7 + offset
        packet.order = order[flowIndex];

        order[flowIndex]++;
        // *** END PACKET GENERATOR ***

        // *** END TIMER ***
        overhead += rdtsc() - startTSC;

        // *** PACKET PROCESSOR ***
        if(expected[packet.flow - offset] != packet.order){
            printf("Out of order");
            exit(1);
        }
        expected[packet.flow - offset]++;
        count[packet.flow - offset]++;
        // *** END PACKET PROCESSOR ***
    }

    records->overhead = overhead;
    for(int i = 0; i < FLOWS_PER_THREAD; i++){
        records->count[i] = count[i];
        records->result[i] = expected[i];
    }

    return NULL;
}

void overview(){
    size_t countTotal = 0;
    size_t overheadTotal = 0;
    data_t returnVal;

    printf("TESTING\n\n");

    //Wait for all threads to signal they are ready
    for(int i = 0; i < THREADS; i++){
        while(readyFlag[i] == 0);
    }

    startFlag = 1;

    usleep(RUNTIME);
    
    endFlag = 1;

    printf("Verifying Results:\n");
    fflush(NULL);
    for(int i = 0; i < THREADS; i++){
        pthread_join(threadID[i], NULL); 
        returnVal = threadData[i];

        //Verify the results
        printf("\nThread %d: \n", i);
        for(int j = 0; j < FLOWS_PER_THREAD; j++){

            overheadTotal += returnVal.overhead;

            if(verify_sequence(i * FLOWS_PER_THREAD + j, returnVal.count[j], returnVal.result[j]) == 1)
                countTotal += returnVal.count[j];
            else
               printf("Incorrect results for flow %d\n", i * FLOWS_PER_THREAD + j);
        }
    }

    //Print out overall data
    printf("\nPackets Generated:\t\t%'ld Packets\n", countTotal);  
    printf("Packets Generated Per Thread:\t%'ld Packets\n", countTotal / THREADS); 
    printf("Runtime:\t\t\t%d Seconds\n", RUNTIME / 1000000); 
    printf("Overhead Ticks:\t\t\t%'ld Ticks\n", overheadTotal); 
    printf("Average Overhead Per Thread:\t%'ld Ticks\n", overheadTotal / THREADS); 
    printf("Average Packet Creation Time:\t%'ld Ticks\n\n", overheadTotal / countTotal); 
}

int main(int argc, char**argv){
    setlocale(LC_NUMERIC, "");

    int numSeq = THREADS;

    //Pin the overview to core 0
    assign_to_zero();

    //Generate the sequences
    generate_sequences(numSeq);

    pthread_attr_init(&attrs);

    //Spawn the threads
    int cores[8] = {3, 4, 5, 6, 7, 8, 9, 10};

    startFlag = 0;
    endFlag = 0;

    for(int i = 0; i < THREADS; i++){
        readyFlag[i] = 0;
    }

    for(int i = 0; i < THREADS; i++){
        //Assign args
        threadData[i].overhead = 0;

        threadArgs[i].results = &threadData[i];
        threadArgs[i].core = cores[i];

        //Spawn the thread
        pthread_create(&threadID[i], &attrs, thread, (void *)&threadArgs[i]);
    }  

    //Keep time
    overview();
}
