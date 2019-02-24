//Contains functions that help with timing/graphics
//Can put other functions here that are necessary for framework

#include<wrapper.h>
#include<global.h>

#if defined(__i386__)

typedef unsigned long long tsc_t;

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

//Purely For Visual Asthetics For Loading Bar
void printLoading(int perc){
    //Delete previous loading bar
    for(int backCount = 0; backCount < CALIBRATION * 2 + 8; backCount++){
        printf("\b");
    }

    //Print next loading bar
    printf("{");
    int prog;
    for(prog = 0; prog < (perc + 1) * 2; prog++){
        printf("#");
    }
    for(int j = prog; j < (CALIBRATION + 1) * 2; j++){
        printf("-");
    }

    //Print Percentage
    printf("}%3d%%", (perc*100)/ CALIBRATION);
}

// Find out roughly how many TSC cycles happen in a second.
// This isn't perfectly accurate - we could get more accuracy by running
// for a longer time and by running this test repeatedly.
tsc_t cal(void){
    //Notify user program is running
    printf("Normalizing Metric...\t");
    printf("{");
    for(int i = 0; i < CALIBRATION * 2; i++){
        printf("-");
    }
    printf("}%3d%%", 0);
    fflush(NULL);

    // Find a rough value for the number of TSC ticks in 1s
    tsc_t total = 0;
    int i;
    for(i = 0; i < CALIBRATION; i++){
        tsc_t start=rdtsc();
        usleep(CAL_DUR);
        tsc_t end=rdtsc();
        total += ((end-start)*1000000ULL)/(CAL_DUR * CALIBRATION);
        printLoading(i);
        fflush(NULL);
    }
    printLoading(i);
    printf("\n");
    return total;
}