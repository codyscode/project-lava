// For timing purposes we're going to use the x64 in-chip timestamp counter
// Per core, this counter is a very high resolution fixed-frequency timer
// that steadily increments, and it's very cheap to read using the 'rdtsc'
// assembly instruction (see the 'rdtsc' function).

// You'll also notice use of the 'mfence' instruction here along with
// some specific functions that force gcc to flush all writes to memory.
// gcc will not necessarily write things to the queue in any particular
// sequence, so we have to force it to clear out data it might still be
// holding in registers and on the stack before we're sure it's going to be
// where another, asynchronous, thread will find it.  The CPU also executes
// instructions, including writes to main memory, out of order, and the
// 'mfence' instruction is used to confirm that the memory has all been
// written before we move on to the next task.  See the 'FENCE()' macro.

// Note that this code does not do packet passing, it's just an example
// to work from.  It's actually intended to measure the realtime behaviour
// of a core in Linux, since other processes will time-share the core
// unless we do specific tuning.  We can use this program as-is to see
// how well your own code is isolated, but the queue used here is one
// example of how queues can be made.  Note that it is only writing
// 64 bit objects, not packets, and because the reader always reads the
// queue before the writer can fill it we can cheat a lot on locking -
// that shortcut won't work for your own test cases, so you will have to
// deal with the case where there's no more space in the queue to put
// a packet.

// Also note something else: this doesn't use system calls in the
// test threads, because system calls don't take a fixed amount of time,
// and switching from userspace to kernel space is expensive.  You might
// want to start with a kernel lock to get a basic queue working, but
// eventually you will need to think about other options.

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>


// How many loops of work we do.
tsc_t work_duration;

// Run a bit of fixed duration work, because we're going to see
// how many TSC ticks this takes to run.  In a perfect world it
// would be completely consistent.  This gives us a baseline for
// working out how consistently we're running these threads and how
// much they're being interrupted.
void work(int thread)
{
    // Some fixed-duration work.
    unsigned long long count;
    for(count=0;count<work_duration;count++) {
	;
    }
}

// Set thread properties - specifically the ones that make this a
// realtime thread, which means it will always be chosen to run
// when considered against non-RT threads such as other normal
// Unix processes.  This means that we should be the only thing
// that uses the core we pick.  It also assigns it to one core,
// so that it doesn't move around and invalidate the L1 cache.
void set_thread_props(int tgt_core)
{
    pthread_t self = pthread_self();

    const struct sched_param params = {
	.sched_priority = 2
    };
    if(pthread_setschedparam(self, SCHED_FIFO, &params)) {
	    perror("pthread_setschedparam");
    }

    // Repin this thread to one specific core
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(tgt_core, &cpuset);

    if(pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
	    perror("pthread_attr_setaffinity_np");
    }
}

#define FENCE() \
    asm volatile ("mfence" ::: "memory");

// This is our queue space.  We write data here.  In this case, we
// are writing TSC values (64 bit integers).  Because we never check
// to see if the queue is full, we don't need to keep a last-read
// counter - we can always assume the reader has read the spot we're
// about to write.
#define OUTSIZE 10000
struct tsout {
    int core; // thread-specific settings are done in the startup of the thread.  Among other things, they don't seem to work via attrs.
    int thread;
    size_t last_written;
    tsc_t times[OUTSIZE];
};

// Our worker thread, which tests how long the work() function takes
// to run.  Every run, it writes a timestamp out to the queue.
void *thread(void *arg)
{
    struct tsout *q = (struct tsout *)arg;
    const int threadid=q->thread;
    register size_t slot=0;

    set_thread_props(q->core);

    fprintf(stderr, "Started thread %u\n", threadid);

    while(1) {
        
        work(threadid);

        register tsc_t last=rdtsc();
        q->times[slot]=last;
        // Write the data in the queue first...
            FENCE();

        // .. and then tell the receiving thread new data is available...
        q->last_written=slot;
        // ..so that when the far end sees the update of the pointer, the data is definitely there.
        asm volatile ("mfence" ::: "memory");

        slot++;
        // Circular buffer - loop back to the start when we reach the end.
        if(slot >= OUTSIZE) {
            slot = 0;
        }
    }
}

// Find out roughly how many TSC cycles happen in a second.
// This isn't perfectly accurate - we could get more accuracy by running
// for a longer time and by running this test repeatedly.
tsc_t cal(void)
{

// Microsecond calibration duration.
#define CAL_DUR 2000000ULL

    // Find a rough value for the number of TSC ticks in 1s
    tsc_t start=rdtsc();
    usleep(CAL_DUR);
    tsc_t end=rdtsc();

    return (end-start)*1000000ULL/CAL_DUR;
}

// CLI command: 'rt-test first-core second-core third-core ...'
// Every core we name is used to run a test process.
// The first core of a socket is a bad choice because it receives hardware
// interrupts, but we can tune the OS so that none of the other cores get
// them.  Also remember that there are two sockets in the system and they have
// separate L2 and L3 cache, so you will get different results if you use
// cores from both sockets.
#define MAX_NTHREADS 60
int main(int argc, char **argv)
{
    // For this program, we use one queue per test thread to make sure they
    // aren't interfering with each other.
    struct tsout outs[MAX_NTHREADS];
    pthread_t threads[MAX_NTHREADS];
    // Every time we read 'last_written' we steal the cache line from the
    // test thread, so we try not to do that too often.
    unsigned last_read[MAX_NTHREADS];

    size_t ct;
    const size_t nthreads = argc-1;

    assert(nthreads < MAX_NTHREADS);
    assert(nthreads > 0);

    // We have a 'supervisor' thread that isn't realtime, it just outputs
    // what we learn.  'rt-test > filename.csv' to save it.
    tsc_t calval = cal();
    // First line of output says how many TSC ticks in a second.
    printf("cal,%llu\n",calval);

    // The 'work' process does a tight loop to burn a bit of CPU (and
    // a bit of time).  This is chosen by experiment to be a few tens of ms,
    // but any number will do.
    work_duration = calval/7000;

    pthread_attr_t attrs;

    pthread_attr_init(&attrs);

    for(ct=0;ct<nthreads;ct++) {
        int core = atoi(argv[1+ct]);
        fprintf(stderr, "Placing thread %u on core %u\n", ct, core);

        // We are going to choose how this thread is scheduled by Linux, so
        // it shouldn't just inherit the process's properties.
        if(pthread_attr_setinheritsched(&attrs, PTHREAD_EXPLICIT_SCHED)) {
            perror("pthread_attr_setinheritsched");
        }

        // Reset the queue.
        outs[ct].last_written=OUTSIZE-1;
        outs[ct].thread=ct;
        outs[ct].core = core;
        last_read[ct]=OUTSIZE-1;

        // Make sure that's all in memory before the other thread (on another core) reads it.
        FENCE();

        // Run a worker thread.
        int x;
        if(x = pthread_create(&threads[ct], &attrs, thread, &outs[ct])) {
            fprintf(stderr, "pthread_create: %d\n", x);
        }
    }

    while(1) {
        // Some pause reasonable for human expectations
        // that doesn't allow the thread buffers to fill.
            // Really small times will cause us to check the queues
            // more often, and really long times will allow the queues to overflow.
        usleep(20000); // 20ms

        // Get values out of threads and output them for later analysis.
        for(ct=0;ct<nthreads;ct++) {
            struct tsout *q = &outs[ct];
            size_t end = q->last_written;
            unsigned long long *buf = &q->times[0];
            if(end != last_read[ct]) {
                size_t f = (last_read[ct]+1) % OUTSIZE;

                while(1) {
                    printf("%u,%llu\n", ct, buf[f]);
                    if(end == f) {
                        break;
                    }
                    f = (f+1)%OUTSIZE;
                }
                last_read[ct] = end;
            }
        }
        fflush(stdout);
    }
}
