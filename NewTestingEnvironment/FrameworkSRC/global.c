//Contains functions that help with timing/graphics
//Can put other functions here that are necessary for framework

#include<global.h>
#include<wrapper.h>

// Get the current value of the TSC.  This is a rolling 64 bit counter
// and its frequency varies from across x64 CPU models so we have to
// calibrate it.

#if defined(__i386__)

// Not actually used - most chips are x64 nowadays.
static inline tsc_t rdtsc(void)
{
    register tsc_t x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static inline tsc_t rdtsc(void)
{
    register unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (tsc_t)lo)|( ((tsc_t)hi)<<32 );
}

#endif

// Set thread properties - specifically the ones that make this a
// realtime thread, which means it will always be chosen to run
// when considered against non-RT threads such as other normal
// Unix processes.  This means that we should be the only thing
// that uses the core we pick.  It also assigns it to one core,
// so that it doesn't move around and invalidate the L1 cache.
void set_thread_props(int tgt_core, long sched){
    pthread_t self = pthread_self();
    
    if(sched != (long)NULL){
        const struct sched_param params = {
	        .sched_priority = sched
        };
        if(pthread_setschedparam(self, SCHED_FIFO, &params)) {
	        perror("pthread_setschedparam");
        }
    }

    //Repin this thread to one specific core
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(tgt_core, &cpuset);

    if(pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
	    perror("pthread_attr_setaffinity_np");
    }
}

void sig_alrm(int signo){    
    endFlag = 1;

    for(int i = 0; i < inputThreadCount; i++){
        pthread_cancel(input[i].threadID);
    }
    for(int i = 0; i < outputThreadCount; i++){
        pthread_cancel(output[i].threadID);
    }

    printf("\rTime Remaining:  0 Seconds  ");
    printf("\n\nNote: Your Threads are canceled with pthred_cancel().\nTo modify your thread cleanup handler upon recieving a termination signal, see pthread_cleanup_push()\n\n");
    printf("Finished. Waiting for thread cleanup...\n\n");

    //Take a snapshot of the count
    for(int i = 0; i < MAX_NUM_OUTPUT_THREADS; i++){
        finalTotal += output[i].count;
    }
}

void alarm_init(){
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
}

void alarm_start(){
	alarm(RUNTIME); // set alarm for RUNTIME seconds
    startFlag = 1; // start moving packets
}