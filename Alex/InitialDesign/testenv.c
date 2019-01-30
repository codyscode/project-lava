/*
Used for testing out verious code segments
*/
#include<stdio.h>
#include<stdlib.h>

typedef unsigned long long tsc_t;

static __inline__ tsc_t rdtsc(void)
{
    register unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (tsc_t)lo)|( ((tsc_t)hi)<<32 );
}

typedef struct Input{
    void (*function)();
    int *speed;
}inputArgs;

typedef struct Test{
    int t;
    int p;
}testInput;

void testMethod(inputArgs *arg){
    inputArgs *arguments = (inputArgs *)arg;
    void (*function)(int *) = arguments->function;  
    int *speedArg = arguments->speed;
    (*function)(speedArg);
    printf("Test");
}

void test2(int *array){
    printf("Hello %d %d %d %d", array[0], array[1], array[2], array[3]);
}

void test3(void *arg){
    testInput *arguments = (testInput *)arg;
    printf("Test: %d: ", arguments->t);
}

int main(void){
    tsc_t startTime, doneTime;
    for (int i = 0; i < 20; i++){
        int s = 1;
        startTime = rdtsc();
        doneTime = rdtsc();
        testInput *t;
        t->t = 1;
        t->p = 2;
        test3((void *)t);
        printf("rng: %d\ntime: %llu\n", s, doneTime - startTime);
    }
    void (*funcPtr) = &test2;
    int otherArgs[] = {10, 9, 8, 7};
    inputArgs cmdInputs;
    cmdInputs.function = funcPtr;
    cmdInputs.speed = otherArgs;
    testMethod(&cmdInputs);
}