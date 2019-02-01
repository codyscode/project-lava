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
    int l;
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

void test3(testInput *arg){
    testInput *arguments = (testInput *)arg;
    printf("Test: %d: ", (*arguments).l);
}

void test4(int arg[]){
    int toPass[] = {22, 22, 22};
    arg[0] = toPass[1];
}

int main(void){
    tsc_t startTime, doneTime;
    int testArray[] = {0, 0, 0, 0, 0};
    int testV;
    for (int i = 0; i < 20; i++){
        int index = i;
        index = index % 5;
        testArray[index]++;
        testV = testArray[index];
        /*
        startTime = rdtsc();
        int s = rand() % 18;
        doneTime = rdtsc();
        testInput t;
        t.l = 1;
        t.p = 2;
        test3(&t);
        printf("rng: %d\ntime: %llu\n", s, doneTime - startTime);
        */

        printf("%d\n", testArray[index]);
    }
    test4(testArray);
    printf("outside: %d\n", testArray[0]);
    /*
    void *funcPtr = &test2;
    int otherArgs[] = {10, 9, 8, 7};
    inputArgs cmdInputs;
    cmdInputs.function = funcPtr;
    cmdInputs.speed = otherArgs;
    testMethod(&cmdInputs);
    */
}